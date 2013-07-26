/** libIntegra multimedia module interface
 *
 * Copyright (C) 2007 Birmingham City University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include "platform_specifics.h"

extern "C" 
{
#include <dlfcn.h>
}

#include <sys/stat.h>

#include <libxml/xmlreader.h>

#include "server.h"
#include "scratch_directory.h"
#include "osc_client.h"
#include "system_class_handlers.h"
#include "reentrance_checker.h"
#include "module_manager.h"
#include "trace.h"
#include "globals.h"
#include "xmlrpc_server.h"
#include "helper.h"
#include "value.h"
#include "path.h"
#include "server_commands.h"
#include "bridge_host.h"

#include "api/server_startup_info.h"


using namespace ntg_api;


namespace ntg_api
{
	CServerApi *CServerApi::create_server( const ntg_api::CServerStartupInfo &startup_info )
	{
		NTG_TRACE_PROGRESS_WITH_STRING( "libIntegra version", ntg_version().c_str() );

		#ifdef __APPLE__
			sem_abyss_init = sem_open("sem_abyss_init", O_CREAT, 0777, 0);
			sem_system_shutdown = sem_open("sem_system_shutdown", O_CREAT, 0777, 0);
		#else
			sem_init(&sem_abyss_init, 0, 0);
			sem_init(&sem_system_shutdown, 0, 0);
		#endif

		if( startup_info.bridge_path.empty() ) 
		{
			NTG_TRACE_ERROR("bridge_path is empty" );
			return NULL;
		}

		struct stat file_buffer;
		if( stat( startup_info.bridge_path.c_str(), &file_buffer ) != 0 ) 
		{
			NTG_TRACE_ERROR( "bridge_path points to a nonexsitant file" );
			return NULL;
		}

		if( startup_info.system_module_directory.empty() ) 
		{
			NTG_TRACE_ERROR("system_module_directory is empty" );
			return NULL;
		}

		if( startup_info.third_party_module_directory.empty() ) 
		{
			NTG_TRACE_ERROR("third_party_module_directory is empty" );
			return NULL;
		}

		return new ntg_internal::CServer( startup_info );
	}
}


namespace ntg_internal
{
	#ifdef _WINDOWS
		static void invalid_parameter_handler( const wchar_t * expression, const wchar_t * function, const wchar_t * file, unsigned int line, uintptr_t pReserved )
		{
			NTG_TRACE_ERROR( "CRT encoutered invalid parameter!" );
		}
	#endif


	
	static void host_callback( internal_id id, const char *attribute_name, const CValue *value )
	{
		if( server_->get_terminate_flag() ) 
		{
			return;
		}

		server_->lock();

		const CNode *target = server_->find_node( id );
		if( target ) 
		{
			CPath path( target->get_path() );
			path.append_element( attribute_name );

			ntg_set_( *server_, NTG_SOURCE_HOST, path, value );
		}
		else
		{
			NTG_TRACE_ERROR_WITH_INT("couldn't find node with id", id );
		}

		server_->unlock();
	}


	CServer::CServer( const ntg_api::CServerStartupInfo &startup_info )
	{
		NTG_TRACE_PROGRESS( "constructing server" );

		server_ = this;

		pthread_mutex_init( &m_mutex, NULL );

		#ifdef _WINDOWS
			_set_invalid_parameter_handler( invalid_parameter_handler );
		#endif

		m_scratch_directory = new CScratchDirectory;

		m_module_manager = new CModuleManager( get_scratch_directory(), startup_info.system_module_directory, startup_info.third_party_module_directory );

		m_osc_client = ntg_osc_client_new( startup_info.osc_client_url.c_str(), startup_info.osc_client_port );
		m_terminate = false;

		ntg_system_class_handlers_initialize( *this );

		m_reentrance_checker = new CReentranceChecker();

		m_bridge = ( ntg_bridge_interface * ) ntg_bridge_load( startup_info.bridge_path.c_str() );
		if( m_bridge ) 
		{
			m_bridge->bridge_init();
		} 
		else 
		{
			NTG_TRACE_ERROR( "bridge failed to load" );
		}

		/* Add the server receive callback to the bridge's methods */
		m_bridge->server_receive_callback = host_callback;

		/* create the xmlrpc interface */
		unsigned short *xmlport = new unsigned short;
		*xmlport = startup_info.xmlrpc_server_port;
		pthread_create( &m_xmlrpc_thread, NULL, ntg_xmlrpc_server_run, xmlport );

	#ifndef _WINDOWS
		ntg_sig_setup();
	#endif

		NTG_TRACE_PROGRESS( "Server construction complete" );
	}


	CServer::~CServer()
	{
		NTG_TRACE_PROGRESS("setting terminate flag");

		lock();
		m_terminate = true; /* FIX: use a semaphore or condition */

		/* delete all nodes */
		node_map copy_of_nodes = m_nodes;
		for( node_map::const_iterator i = copy_of_nodes.begin(); i != copy_of_nodes.end(); i++ )
		{
			ntg_delete_( *this, NTG_SOURCE_SYSTEM, i->second->get_path() );
		}
	
		/* shutdown system class handlers */
		ntg_system_class_handlers_shutdown( *this );

		/* de-reference bridge */
	    m_bridge = NULL;

		delete m_module_manager;

		delete m_reentrance_checker;

		delete m_scratch_directory;

		NTG_TRACE_PROGRESS( "shutting down OSC client" );
		ntg_osc_client_destroy( m_osc_client );
		
		NTG_TRACE_PROGRESS("shutting down XMLRPC interface");
		ntg_xmlrpc_server_terminate();

		/* FIX: for now we only support the old 'stable' xmlrpc-c, which can't
		   wake up a sleeping server */
		NTG_TRACE_PROGRESS("joining xmlrpc thread");
		pthread_join( m_xmlrpc_thread, NULL );



		/* FIX: This hangs on all platforms, just comment out for now */
		/*
		NTG_TRACE_PROGRESS("closing bridge");
		dlclose(bridge_handle);
		*/

		NTG_TRACE_PROGRESS( "cleaning up XML parser" );
		xmlCleanupParser();
		xmlCleanupGlobals();


		NTG_TRACE_PROGRESS( "done!" );

		server_ = NULL;

		unlock();

		pthread_mutex_destroy( &m_mutex );

		NTG_TRACE_PROGRESS( "server destruction complete" );
	}


	void CServer::block_until_shutdown_signal()
	{
		NTG_TRACE_PROGRESS("server blocking until shutdown signal...");

		sem_wait( SEM_SYSTEM_SHUTDOWN );

		NTG_TRACE_PROGRESS("server blocking finished...");
	}


	void CServer::lock()
	{
	    pthread_mutex_lock( &m_mutex );
	}


	void CServer::unlock()
	{
		pthread_mutex_unlock( &m_mutex );
	}


	const CNode *CServer::find_node( const ntg_api::string &path_string, const CNode *relative_to ) const
	{
		if( relative_to )
		{
			return m_state_table.lookup_node( relative_to->get_path().get_string() + "." + path_string );
		}
		else
		{
			return m_state_table.lookup_node( path_string );
		}
	}


	const CNode *CServer::find_node( internal_id id ) const
	{
		return m_state_table.lookup_node( id );
	}


	CNode *CServer::find_node_writable( const ntg_api::string &path_string, const CNode *relative_to )
	{
		if( relative_to )
		{
			return m_state_table.lookup_node_writable( relative_to->get_path().get_string() + "." + path_string );
		}
		else
		{
			return m_state_table.lookup_node_writable( path_string );
		}
	}


	const node_map &CServer::get_sibling_set( const CNode &node ) const
	{
		const CNode *parent = node.get_parent();
		if( parent )
		{
			return parent->get_children();
		}
		else
		{
			return m_nodes;
		}
	}


	node_map &CServer::get_sibling_set_writable( CNode &node )
	{
		CNode *parent = node.get_parent_writable();
		if( parent )
		{
			return parent->get_children_writable();
		}
		else
		{
			return m_nodes;
		}
	}


	const CNodeEndpoint *CServer::find_node_endpoint( const ntg_api::string &path_string, const CNode *relative_to ) const
	{
		if( relative_to )
		{
			return m_state_table.lookup_node_endpoint( relative_to->get_path().get_string() + "." + path_string );
		}
		else
		{
			return m_state_table.lookup_node_endpoint( path_string );
		}
	}


	CNodeEndpoint *CServer::find_node_endpoint_writable( const ntg_api::string &path_string, const CNode *relative_to )
	{
		if( relative_to )
		{
			return m_state_table.lookup_node_endpoint_writable( relative_to->get_path().get_string() + "." + path_string );
		}
		else
		{
			return m_state_table.lookup_node_endpoint_writable( path_string );
		}
	}


	const string &CServer::get_scratch_directory() const
	{
		return m_scratch_directory->get_scratch_directory();
	}



	
}


