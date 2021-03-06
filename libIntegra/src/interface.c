/** libIntegra multimedia module interface
 *
 * Copyright (C) 2012 Birmingham City University
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "platform_specifics.h"

#include <assert.h>

#include <libxml/xmlreader.h>

#include "interface.h"
#include "Integra/integra.h"
#include "memory.h"
#include "helper.h"
#include "value.h"


#define INTERFACE					"InterfaceDeclaration"
#define INTERFACE_INFO				"InterfaceInfo"			
#define NAME						"Name"
#define LABEL						"Label"
#define DESCRIPTION					"Description"
#define TAGS						"Tags"
#define TAG							"Tag"
#define IMPLEMENTED_IN_LIBINTEGRA	"ImplementedInLibIntegra"
#define AUTHOR						"Author"
#define CREATED_DATE				"CreatedDate"
#define MODIFIED_DATE				"ModifiedDate"
#define ENDPOINT_INFO				"EndpointInfo"
#define ENDPOINT					"Endpoint"
#define TYPE						"Type"
#define CONTROL_INFO				"ControlInfo"
#define CONTROL_TYPE				"ControlType"
#define STATE_INFO					"StateInfo"
#define STATE_TYPE					"StateType"
#define CONSTRAINT					"Constraint"
#define RANGE						"Range"
#define MINIMUM						"Minimum"
#define MAXIMUM						"Maximum"
#define SCALE						"Scale"
#define SCALE_TYPE					"ScaleType"
#define BASE						"Base"
#define ALLOWED_STATES				"AllowedStates"
#define STATE						"State"
#define DEFAULT						"Default"
#define STATE_LABELS				"StateLabels"
#define LABEL						"Label"
#define TEXT						"Text"
#define IS_INPUT_FILE				"IsInputFile"
#define IS_SAVED_TO_FILE			"IsSavedToFile"
#define CAN_BE_SOURCE				"CanBeSource"
#define CAN_BE_TARGET				"CanBeTarget"
#define IS_SENT_TO_HOST				"IsSentToHost"
#define STREAM_INFO					"StreamInfo"
#define STREAM_TYPE					"StreamType"
#define STREAM_DIRECTION			"StreamDirection"
#define WIDGET_INFO					"WidgetInfo"
#define WIDGET						"Widget"
#define WIDGET_TYPE					"WidgetType"
#define POSITION					"Position"
#define X							"X"
#define Y							"Y"
#define WIDTH						"Width"
#define HEIGHT						"Height"
#define ATTRIBUTE_MAPPINGS			"AttributeMappings"
#define ATTRIBUTE_MAPPING			"AttributeMapping"
#define WIDGET_ATTRIBUTE			"WidgetAttribute"
#define IMPLEMENTATION_INFO			"ImplementationInfo"
#define PATCH_NAME					"PatchName"

#define NTG_STR_FALSE				"false"
#define NTG_STR_TRUE				"true"
#define NTG_STR_CONTROL				"control"
#define NTG_STR_STREAM				"stream"
#define NTG_STR_STATE				"state"
#define NTG_STR_BANG				"bang"
#define NTG_STR_FLOAT				"float"
#define NTG_STR_INTEGER				"integer"
#define NTG_STR_STRING				"string"
#define NTG_STR_LINEAR				"linear"
#define NTG_STR_EXPONENTIAL			"exponential"
#define NTG_STR_DECIBEL				"decibel"
#define NTG_STR_AUDIO				"audio"
#define NTG_STR_INPUT				"input"
#define NTG_STR_OUTPUT				"output"

#define NTG_ATTRIBUTE_MODULE_GUID	"moduleGuid"
#define NTG_ATTRIBUTE_ORIGIN_GUID	"originGuid"

#define DOT "."

#define NTG_CORE_TAG "core"

/* 
 the following macros handle reversing the order of lists

 NTG_IMPLEMENT_LIST_REVERSE implements a recursive function in the form <list type> *ntg_tag_list_tail( <list type> *list_head )
 these functions reverse the order of the list, and return a pointer to the last new list head
 
 NTG_REVERSE_LIST can be called for any struct for which NTG_IMPLEMENT_LIST_REVERSE is declared
*/

#define NTG_IMPLEMENT_LIST_REVERSE( STRUCT )										\
	STRUCT *STRUCT##_list_reverse( STRUCT *item, STRUCT *previous_item )			\
	{																				\
		STRUCT *list_tail;															\
		if( !item )		return NULL;												\
		if( item->next )															\
		{																			\
			list_tail = STRUCT##_list_reverse( item->next, item );					\
		}																			\
		else																		\
		{																			\
			list_tail = item;														\
		}																			\
		item->next = previous_item;													\
		return list_tail;															\
	}																

NTG_IMPLEMENT_LIST_REVERSE( ntg_tag )
NTG_IMPLEMENT_LIST_REVERSE( ntg_endpoint )
NTG_IMPLEMENT_LIST_REVERSE( ntg_allowed_state )
NTG_IMPLEMENT_LIST_REVERSE( ntg_state_label )
NTG_IMPLEMENT_LIST_REVERSE( ntg_widget )
NTG_IMPLEMENT_LIST_REVERSE( ntg_widget_attribute_mapping )

#define NTG_REVERSE_LIST( LIST, STRUCT ) LIST = STRUCT##_list_reverse( LIST, NULL )


/* 
 the following macros handle creation of structs within the loaded interface structure, and are
 to be used only by ntg_handle_interface_element.  
*/


#define NTG_CREATE_CHILD_OBJECT( START_TAG, STORAGE_POINTER, ALLOCATOR )					\
	if( !strcmp( element, START_TAG ) )														\
	{																						\
		if( STORAGE_POINTER )																\
		{																					\
			NTG_TRACE_ERROR( "multiple " START_TAG " tags" );								\
			return NTG_ERROR;																\
		}																					\
		else																				\
		{																					\
			STORAGE_POINTER = ALLOCATOR;													\
			return NTG_NO_ERROR;															\
		}																					\
	}	


#define NTG_CREATE_LIST_ENTRY( START_TAG, LIST_HEAD, STRUCT )								\
	if( !strcmp( element, START_TAG ) )														\
	{																						\
		STRUCT *entry = ntg_calloc( 1, sizeof( STRUCT ) );									\
		if( LIST_HEAD )																		\
		{																					\
			entry->next = LIST_HEAD;														\
		}																					\
		LIST_HEAD = entry;																	\
		return NTG_NO_ERROR;																\
	}


#define DEFAULT_ALLOCATOR( STRUCT )															\
	ntg_calloc( 1, sizeof( STRUCT ) )


/*
 the following forward-declarations define functions used by NTG_READ_XXX macros
*/

ntg_error_code ntg_string_converter( const char *input, char **output );
ntg_error_code ntg_boolean_converter( const char *input, bool *output );
ntg_error_code ntg_integer_converter( const char *input, int *output );
ntg_error_code ntg_float_converter( const char *input, float *output );
ntg_error_code ntg_endpoint_type_converter( const char *input, ntg_endpoint_type *output );
ntg_error_code ntg_control_type_converter( const char *input, ntg_control_type *output );
ntg_error_code ntg_value_type_converter( const char *input, ntg_value_type *output );
ntg_error_code ntg_stream_type_converter( const char *input, ntg_stream_type *output );
ntg_error_code ntg_stream_direction_converter( const char *input, ntg_stream_direction *output );

	/* this subset converts to ntg_value objects for each supported type */
ntg_error_code ntg_value_float_converter( const char *input, ntg_value **output );
ntg_error_code ntg_value_integer_converter( const char *input, ntg_value **output );
ntg_error_code ntg_value_string_converter( const char *input, ntg_value **output );

/* 
 the following macros handle reading of xml properties into specified locations, and are 
 to be used only by ntg_handle_interface_element_value.  
*/

#define NTG_READ_POINTER( ELEMENT, STORAGE_LOCATION, CONVERTER )							\
	if( !strcmp( element, ELEMENT ) )														\
	{																						\
		if( STORAGE_LOCATION )																\
		{																					\
			assert( false );																\
			NTG_TRACE_ERROR_WITH_STRING( "multiple tags", ELEMENT );						\
			return NTG_ERROR;																\
		}																					\
		else																				\
		{																					\
			return CONVERTER( value, &STORAGE_LOCATION );									\
			return NTG_NO_ERROR;															\
		}																					\
	}

#define NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, CONVERTER )						\
	if( !strcmp( element, ELEMENT ) )														\
	{																						\
		return( CONVERTER( value, &STORAGE_LOCATION ) );									\
	}																						

#define NTG_READ_STRING( ELEMENT, STORAGE_LOCATION )										\
	NTG_READ_POINTER( ELEMENT, STORAGE_LOCATION, ntg_string_converter )

#define NTG_READ_BOOLEAN( ELEMENT, STORAGE_LOCATION )										\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_boolean_converter )

#define NTG_READ_INT( ELEMENT, STORAGE_LOCATION )										\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_integer_converter )

#define NTG_READ_FLOAT( ELEMENT, STORAGE_LOCATION )											\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_float_converter )

#define NTG_READ_DATE( ELEMENT, STORAGE_LOCATION )											\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_string_to_date )

#define NTG_READ_ENDPOINT_TYPE( ELEMENT, STORAGE_LOCATION )									\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_endpoint_type_converter )

#define NTG_READ_CONTROL_TYPE( ELEMENT, STORAGE_LOCATION )									\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_control_type_converter )

#define NTG_READ_STATE_TYPE( ELEMENT, STORAGE_LOCATION )									\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_value_type_converter )

#define NTG_READ_SCALE_TYPE( ELEMENT, STORAGE_LOCATION )									\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_scale_type_converter )

#define NTG_READ_STREAM_TYPE( ELEMENT, STORAGE_LOCATION )									\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_stream_type_converter )

#define NTG_READ_STREAM_DIRECTION( ELEMENT, STORAGE_LOCATION )								\
	NTG_READ_NON_POINTER( ELEMENT, STORAGE_LOCATION, ntg_stream_direction_converter )

#define NTG_READ_VALUE( ELEMENT, STORAGE_LOCATION, VALUE_GETTER )							\
	if( !strcmp( element, ELEMENT ) )														\
	{																						\
		switch( VALUE_GETTER )																\
		{																					\
			case NTG_FLOAT:																	\
				NTG_READ_POINTER( ELEMENT, STORAGE_LOCATION, ntg_value_float_converter )	\
				break;																		\
			case NTG_INTEGER:																\
				NTG_READ_POINTER( ELEMENT, STORAGE_LOCATION, ntg_value_integer_converter )	\
				break;																		\
			case NTG_STRING:																\
				NTG_READ_POINTER( ELEMENT, STORAGE_LOCATION, ntg_value_string_converter )	\
				break;																		\
			default:																		\
				NTG_TRACE_ERROR_WITH_INT( "Unhandled Value Type", VALUE_GETTER );			\
				return NTG_ERROR;															\
		}																					\
	}


/* 
 the following functions are used to free the interface structs 
*/

void ntg_range_free( ntg_range *range )
{
	assert( range );

	if( range->minimum ) ntg_value_free( range->minimum );
	if( range->maximum ) ntg_value_free( range->maximum );

	ntg_free( range );
}


void ntg_allowed_states_list_free( ntg_allowed_state *allowed_states_list )
{
	assert( allowed_states_list );

	if( allowed_states_list->next )
	{
		ntg_allowed_states_list_free( allowed_states_list->next );
	}

	if( allowed_states_list->value ) ntg_value_free( allowed_states_list->value );

	ntg_free( allowed_states_list );
}


void ntg_scale_free( ntg_scale *scale )
{
	assert( scale );

	ntg_free( scale );
}


void ntg_state_label_list_free( ntg_state_label *state_label_list )
{
	assert( state_label_list );

	if( state_label_list->next )
	{
		ntg_state_label_list_free( state_label_list->next );
	}

	if( state_label_list->value )	ntg_value_free( state_label_list->value );
	if( state_label_list->text )	ntg_free( state_label_list->text );

	ntg_free( state_label_list );
}


void ntg_state_info_free( ntg_state_info *state_info )
{
	assert( state_info );

	if( state_info->constraint.range )
	{
		ntg_range_free( state_info->constraint.range );
	}

	if( state_info->constraint.allowed_states )
	{
		ntg_allowed_states_list_free( state_info->constraint.allowed_states );
	}

	if( state_info->default_value )
	{
		ntg_value_free( state_info->default_value );
	}

	if( state_info->scale )
	{
		ntg_scale_free( state_info->scale );
	}

	if( state_info->state_labels )
	{
		ntg_state_label_list_free( state_info->state_labels );
	}

	ntg_free( state_info );
}


void ntg_control_info_free( ntg_control_info *control_info )
{
	assert( control_info );

	if( control_info->state_info )
	{
		ntg_state_info_free( control_info->state_info );
	}

	ntg_free( control_info );
}


void ntg_stream_info_free( ntg_stream_info *stream_info )
{
	assert( stream_info );

	ntg_free( stream_info );
}


void ntg_widget_mapping_list_free( ntg_widget_attribute_mapping *widget_mapping_list )
{
	assert( widget_mapping_list );

	if( widget_mapping_list->next )
	{
		ntg_widget_mapping_list_free( widget_mapping_list->next );
	}

	if( widget_mapping_list->widget_attribute )		ntg_free( widget_mapping_list->widget_attribute );
	if( widget_mapping_list->endpoint )				ntg_free( widget_mapping_list->endpoint );
	
	ntg_free( widget_mapping_list );
}


void ntg_widget_list_free( ntg_widget *widget_list )
{
	assert( widget_list );

	if( widget_list->next )
	{
		ntg_widget_list_free( widget_list->next );
	}

	if( widget_list->type )			ntg_free( widget_list->type );
	if( widget_list->label )		ntg_free( widget_list->label );
	if( widget_list->mapping_list ) ntg_widget_mapping_list_free( widget_list->mapping_list );

	ntg_free( widget_list );
}



void ntg_endpoint_list_free( ntg_endpoint *endpoint_list )
{
	assert( endpoint_list );
	
	if( endpoint_list->next )
	{
		ntg_endpoint_list_free( endpoint_list->next );
	}

	if( endpoint_list->name )			ntg_free( endpoint_list->name );

	if( endpoint_list->label )			ntg_free( endpoint_list->label );
	if( endpoint_list->description )	ntg_free( endpoint_list->description );

	if( endpoint_list->control_info )	ntg_control_info_free( endpoint_list->control_info );
	if( endpoint_list->stream_info )	ntg_stream_info_free( endpoint_list->stream_info );

	ntg_free( endpoint_list );
}


void ntg_tag_list_free( ntg_tag *list )
{
	assert( list );

	if( list->next )
	{
		ntg_tag_list_free( list->next );
	}

	if( list->tag ) ntg_free( list->tag );

	ntg_free( list );
}


void ntg_interface_info_free( ntg_interface_info *info )
{
	assert( info );

	if( info->name )				ntg_free( info->name );
	if( info->label )				ntg_free( info->label );
	if( info->description )			ntg_free( info->description );
	if( info->author )				ntg_free( info->author );

	if( info->tag_list )			ntg_tag_list_free( info->tag_list );

	ntg_free( info );
}


void ntg_implementation_info_free( ntg_implementation_info *implementation_info )
{
	assert( implementation_info );

	if( implementation_info->patch_name )		ntg_free( implementation_info->patch_name );

	ntg_free( implementation_info );
}


void ntg_interface_free( ntg_interface *interface )
{
	assert( interface );

	if( interface->file_path )		ntg_free( interface->file_path );
	if( interface->info )			ntg_interface_info_free( interface->info );
	if( interface->endpoint_list )	ntg_endpoint_list_free( interface->endpoint_list );
	if( interface->widget_list )	ntg_widget_list_free( interface->widget_list );
	if( interface->implementation ) ntg_implementation_info_free( interface->implementation );

	ntg_free( interface );
}


/* 
 the following functions are used by NTG_READ_XXX macros to interpret xml text
*/

ntg_error_code ntg_string_converter( const char *input, char **output )
{
	*output = strdup( input );
	return NTG_NO_ERROR;
}

ntg_error_code ntg_boolean_converter( const char *input, bool *output )
{
	if( !strcmp( input, NTG_STR_FALSE ) )
	{
		*output = false;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_TRUE ) )
	{
		*output = true;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid boolean", input );
	return NTG_ERROR;
}

ntg_error_code ntg_integer_converter( const char *input, int *output )
{
	*output = atoi( input );
	return NTG_NO_ERROR;
}

ntg_error_code ntg_float_converter( const char *input, float *output )
{
	*output = atof( input );
	return NTG_NO_ERROR;
}


ntg_error_code ntg_endpoint_type_converter( const char *input, ntg_endpoint_type *output )
{
	if( !strcmp( input, NTG_STR_CONTROL ) )
	{
		*output = NTG_CONTROL;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_STREAM ) )
	{
		*output = NTG_STREAM;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid endpoint type", input );
	return NTG_ERROR;
}

ntg_error_code ntg_control_type_converter( const char *input, ntg_control_type *output )
{
	if( !strcmp( input, NTG_STR_STATE ) )
	{
		*output = NTG_STATE;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_BANG ) )
	{
		*output = NTG_BANG;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid control type", input );
	return NTG_ERROR;
}

ntg_error_code ntg_value_type_converter( const char *input, ntg_value_type *output )
{
	if( !strcmp( input, NTG_STR_FLOAT ) )
	{
		*output = NTG_FLOAT;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_INTEGER ) )
	{
		*output = NTG_INTEGER;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_STRING ) )
	{
		*output = NTG_STRING;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid value type", input );
	return NTG_ERROR;
}

ntg_error_code ntg_scale_type_converter( const char *input, ntg_scale_type *output )
{
	if( !strcmp( input, NTG_STR_LINEAR ) )
	{
		*output = NTG_LINEAR;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_EXPONENTIAL ) )
	{
		*output = NTG_EXPONENTIAL;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_DECIBEL ) )
	{
		*output = NTG_DECIBEL;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid scale type", input );
	return NTG_ERROR;
}

ntg_error_code ntg_stream_type_converter( const char *input, ntg_stream_type *output )
{
	if( !strcmp( input, NTG_STR_AUDIO ) )
	{
		*output = NTG_AUDIO_STREAM;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid stream type", input );
	return NTG_ERROR;
}

ntg_error_code ntg_stream_direction_converter( const char *input, ntg_stream_direction *output )
{
	if( !strcmp( input, NTG_STR_INPUT ) )
	{
		*output = NTG_STREAM_INPUT;
		return NTG_NO_ERROR;
	}

	if( !strcmp( input, NTG_STR_OUTPUT ) )
	{
		*output = NTG_STREAM_OUTPUT;
		return NTG_NO_ERROR;
	}

	NTG_TRACE_ERROR_WITH_STRING( "invalid stream direction", input );
	return NTG_ERROR;
}


ntg_error_code ntg_value_float_converter( const char *input, ntg_value **output )
{
	*output = ntg_value_from_string( NTG_FLOAT, input );
	return NTG_NO_ERROR;
}

ntg_error_code ntg_value_integer_converter( const char *input, ntg_value **output )
{
	*output = ntg_value_from_string( NTG_INTEGER, input );
	return NTG_NO_ERROR;
}

ntg_error_code ntg_value_string_converter( const char *input, ntg_value **output )
{
	*output = ntg_value_from_string( NTG_STRING, input );
	return NTG_NO_ERROR;
}


/*
 the following functions are helpers and workers to load ntg_interface and owned structs from iid files
*/


char *ntg_push_element_name( char *element_path, const char *element_name )
{
	char *result;
	assert( element_name );

	if( element_path )
	{
		result = ntg_malloc( strlen( element_path ) + strlen( DOT ) + strlen( element_name ) + 1 );
		sprintf( result, "%s%s%s", element_path, DOT, element_name );
		ntg_free( element_path );
	}
	else
	{
		result = ntg_strdup( element_name );
	}

	return result;
}


void ntg_pop_element_name( char *element_path )
{
	int i;

	assert( element_path );

	for( i = strlen( element_path ) - 1; i >= 0; i-- )
	{
		if( element_path[ i ] == *DOT )
		{
			element_path[ i ] = 0;
			return;
		}
	}

	/* separator not found - can't pop */
	assert( false );
}


ntg_value_type ntg_get_range_type( ntg_interface *interface )
{
	ntg_value_type type;

	assert( interface && interface->endpoint_list );

	type = interface->endpoint_list->control_info->state_info->type;

	if( type == NTG_STRING )
	{
		/* special case - strings have integer for their range - defines length limits */
		return NTG_INTEGER;
	}
	else
	{
		return type;
	}
}


ntg_value_type ntg_get_state_type( ntg_interface *interface )
{
	assert( interface && interface->endpoint_list );

	return interface->endpoint_list->control_info->state_info->type;
}


ntg_control_info *ntg_control_info_allocator()
{
	ntg_control_info *control_info = ntg_calloc( 1, sizeof( ntg_control_info ) );

	/*initialize non-zero default values*/
	control_info->can_be_source = true;
	control_info->can_be_target = true;
	control_info->is_sent_to_host = true;
	
	return control_info;
}


ntg_state_info *ntg_state_info_allocator()
{
	ntg_state_info *state_info = ntg_calloc( 1, sizeof( ntg_state_info ) );

	/*initialize non-zero default values*/
	state_info->is_saved_to_file = true;
	
	return state_info;
}


/* 
 all lists are loaded in reverse order, because new entries are pushed to the head.  To recreate original order,
 we must reverse the order of all lists
*/
void ntg_reverse_lists( ntg_interface *interface )
{
	ntg_endpoint *endpoint;
	ntg_widget *widget;

	assert( interface );

	NTG_REVERSE_LIST( interface->info->tag_list, ntg_tag );
	NTG_REVERSE_LIST( interface->endpoint_list, ntg_endpoint );
	NTG_REVERSE_LIST( interface->widget_list, ntg_widget );

	for( endpoint = interface->endpoint_list; endpoint; endpoint = endpoint->next )
	{
		if( endpoint->type == NTG_CONTROL && endpoint->control_info->type == NTG_STATE )
		{
			if( endpoint->control_info->state_info->constraint.allowed_states )
			{
				NTG_REVERSE_LIST( endpoint->control_info->state_info->constraint.allowed_states, ntg_allowed_state );
			}

			NTG_REVERSE_LIST( endpoint->control_info->state_info->state_labels, ntg_state_label );
		}
	}

	for( widget = interface->widget_list; widget; widget = widget->next )
	{
		NTG_REVERSE_LIST( widget->mapping_list, ntg_widget_attribute_mapping );
	}
}


/* ntg_sanity_check doesn't test all the schema rules, but it does test the most essential things */
ntg_error_code ntg_sanity_check( const ntg_interface *interface )
{
	const ntg_endpoint *endpoint;
	const ntg_widget *widget;

	if( !interface || !interface->info || !interface->endpoint_list )		return NTG_ERROR;

	if( !interface->info->name )		return NTG_ERROR;

	for( endpoint = interface->endpoint_list; endpoint; endpoint = endpoint->next )
	{
		if( !endpoint->name )	return NTG_ERROR;

		switch( endpoint->type )
		{
			case NTG_CONTROL:	
				if( !endpoint->control_info ) return NTG_ERROR;

				switch( endpoint->control_info->type )
				{
					case NTG_STATE:
						if( !endpoint->control_info->state_info ) return NTG_ERROR;
						
						break;

					case NTG_BANG:
						break;

					default:
						return NTG_ERROR;
				}
				break;

			case NTG_STREAM:
				if( !endpoint->stream_info ) return NTG_ERROR;
				break;

			default:
				return NTG_ERROR;
		}
	}

	for( widget = interface->widget_list; widget; widget = widget->next )
	{
		if( !widget->type || !widget->label )
		{
			return NTG_ERROR;
		}
	}

	return NTG_NO_ERROR;
}


/* sets default values where needed */
void ntg_propogate_defaults( ntg_interface *interface )
{
	ntg_endpoint *endpoint;

	assert( interface );

	if( !interface->info->label )
	{
		interface->info->label = ntg_strdup( interface->info->name );
	}

	for( endpoint = interface->endpoint_list; endpoint; endpoint = endpoint->next )
	{
		if( !endpoint->label ) 
		{
			endpoint->label = ntg_strdup( endpoint->name );
		}
	}
}


/* iterates over endpoints assigning index to each */
void ntg_assign_endpoint_indices( ntg_interface *interface )
{
	ntg_endpoint *endpoint;
	int index;

	assert( interface );

	index = 0;

	for( endpoint = interface->endpoint_list; endpoint; endpoint = endpoint->next )
	{
		endpoint->endpoint_index = index;
		index++;
	}
}


ntg_error_code ntg_handle_interface_element( ntg_interface *interface, const char *element )
{
	assert( interface && element );

	/* handle child objects */

	NTG_CREATE_CHILD_OBJECT( 
		INTERFACE DOT INTERFACE_INFO, 
		interface->info, 
		DEFAULT_ALLOCATOR( ntg_interface_info ) );

	NTG_CREATE_CHILD_OBJECT( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO, 
		interface->endpoint_list->control_info,
		ntg_control_info_allocator() );

	NTG_CREATE_CHILD_OBJECT( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO, 
		interface->endpoint_list->control_info->state_info, 
		ntg_state_info_allocator() );

	NTG_CREATE_CHILD_OBJECT( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT STREAM_INFO, 
		interface->endpoint_list->stream_info, 
		DEFAULT_ALLOCATOR( ntg_stream_info ) );

	NTG_CREATE_CHILD_OBJECT( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT CONSTRAINT DOT RANGE, 
		interface->endpoint_list->control_info->state_info->constraint.range, 
		DEFAULT_ALLOCATOR( ntg_range ) );

	NTG_CREATE_CHILD_OBJECT(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT SCALE,
		interface->endpoint_list->control_info->state_info->scale,
		DEFAULT_ALLOCATOR( ntg_scale ) );

	NTG_CREATE_CHILD_OBJECT( 
		INTERFACE DOT IMPLEMENTATION_INFO, 
		interface->implementation, 
		DEFAULT_ALLOCATOR( ntg_implementation_info ) );

	/* handle list entries */

	NTG_CREATE_LIST_ENTRY( 
		INTERFACE DOT INTERFACE_INFO DOT TAGS DOT TAG, 
		interface->info->tag_list, 
		ntg_tag );

	NTG_CREATE_LIST_ENTRY( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT, 
		interface->endpoint_list, 
		ntg_endpoint );

	NTG_CREATE_LIST_ENTRY( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT CONSTRAINT DOT ALLOWED_STATES DOT STATE, 
		interface->endpoint_list->control_info->state_info->constraint.allowed_states, 
		ntg_allowed_state );

	NTG_CREATE_LIST_ENTRY( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT STATE_LABELS DOT LABEL, 
		interface->endpoint_list->control_info->state_info->state_labels, 
		ntg_state_label );

	NTG_CREATE_LIST_ENTRY( 
		INTERFACE DOT WIDGET_INFO DOT WIDGET, 
		interface->widget_list, 
		ntg_widget );

	NTG_CREATE_LIST_ENTRY( 
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT ATTRIBUTE_MAPPINGS DOT ATTRIBUTE_MAPPING, 
		interface->widget_list->mapping_list, 
		ntg_widget_attribute_mapping );

	return NTG_NO_ERROR;
}



ntg_error_code ntg_handle_interface_element_value( ntg_interface *interface, const char *element, const char *value )
{
	assert( interface && element && value );

	/* interface info */
	NTG_READ_STRING( 
		INTERFACE DOT INTERFACE_INFO DOT NAME, 
		interface->info->name );

	NTG_READ_STRING( 
		INTERFACE DOT INTERFACE_INFO DOT LABEL, 
		interface->info->label );

	NTG_READ_STRING( 
		INTERFACE DOT INTERFACE_INFO DOT DESCRIPTION, 
		interface->info->description );

	NTG_READ_STRING( 
		INTERFACE DOT INTERFACE_INFO DOT TAGS DOT TAG, 
		interface->info->tag_list->tag );

	NTG_READ_BOOLEAN( 
		INTERFACE DOT INTERFACE_INFO DOT IMPLEMENTED_IN_LIBINTEGRA, 
		interface->info->implemented_in_libintegra );

	NTG_READ_STRING( 
		INTERFACE DOT INTERFACE_INFO DOT AUTHOR, 
		interface->info->author );

	NTG_READ_DATE( 
		INTERFACE DOT INTERFACE_INFO DOT CREATED_DATE, 
		interface->info->created_date );

	NTG_READ_DATE( 
		INTERFACE DOT INTERFACE_INFO DOT MODIFIED_DATE, 
		interface->info->modified_date );


	/* endpoint info */
	NTG_READ_STRING( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT NAME, 
		interface->endpoint_list->name );

	NTG_READ_STRING( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT LABEL, 
		interface->endpoint_list->label );

	NTG_READ_STRING( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT DESCRIPTION, 
		interface->endpoint_list->description );

	NTG_READ_ENDPOINT_TYPE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT TYPE, 
		interface->endpoint_list->type );

	NTG_READ_CONTROL_TYPE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT CONTROL_TYPE, 
		interface->endpoint_list->control_info->type );

	NTG_READ_STATE_TYPE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT STATE_TYPE, 
		interface->endpoint_list->control_info->state_info->type );

	NTG_READ_VALUE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT CONSTRAINT DOT RANGE DOT MINIMUM,
		interface->endpoint_list->control_info->state_info->constraint.range->minimum,
		ntg_get_range_type( interface ) );

	NTG_READ_VALUE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT CONSTRAINT DOT RANGE DOT MAXIMUM,
		interface->endpoint_list->control_info->state_info->constraint.range->maximum,
		ntg_get_range_type( interface ) );

	NTG_READ_VALUE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT CONSTRAINT DOT ALLOWED_STATES DOT STATE,
		interface->endpoint_list->control_info->state_info->constraint.allowed_states->value,
		ntg_get_state_type( interface ) );
	
	NTG_READ_VALUE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT DEFAULT,
		interface->endpoint_list->control_info->state_info->default_value,
		ntg_get_state_type( interface ) );

	NTG_READ_SCALE_TYPE(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT SCALE DOT SCALE_TYPE,
		interface->endpoint_list->control_info->state_info->scale->scale_type );

	NTG_READ_INT(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT SCALE DOT BASE,
		interface->endpoint_list->control_info->state_info->scale->exponent_root );

	NTG_READ_VALUE( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT STATE_LABELS DOT LABEL DOT STATE,
		interface->endpoint_list->control_info->state_info->state_labels->value,
		ntg_get_state_type( interface ) );

	NTG_READ_STRING( 
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT STATE_LABELS DOT LABEL DOT TEXT,
		interface->endpoint_list->control_info->state_info->state_labels->text );

	NTG_READ_BOOLEAN(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT IS_INPUT_FILE,
		interface->endpoint_list->control_info->state_info->is_input_file );

	NTG_READ_BOOLEAN(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT STATE_INFO DOT IS_SAVED_TO_FILE,
		interface->endpoint_list->control_info->state_info->is_saved_to_file );

	NTG_READ_BOOLEAN(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT CAN_BE_SOURCE,
		interface->endpoint_list->control_info->can_be_source );

	NTG_READ_BOOLEAN(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT CAN_BE_TARGET,
		interface->endpoint_list->control_info->can_be_target );

	NTG_READ_BOOLEAN(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT CONTROL_INFO DOT IS_SENT_TO_HOST,
		interface->endpoint_list->control_info->is_sent_to_host );

	NTG_READ_STREAM_TYPE(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT STREAM_INFO DOT STREAM_TYPE,
		interface->endpoint_list->stream_info->type );

	NTG_READ_STREAM_DIRECTION(
		INTERFACE DOT ENDPOINT_INFO DOT ENDPOINT DOT STREAM_INFO DOT STREAM_DIRECTION,
		interface->endpoint_list->stream_info->direction );


	/* widget info */

	NTG_READ_STRING( 
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT WIDGET_TYPE,
		interface->widget_list->type );

	NTG_READ_STRING(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT LABEL,
		interface->widget_list->label );

	NTG_READ_FLOAT(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT POSITION DOT X,
		interface->widget_list->position.x );

	NTG_READ_FLOAT(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT POSITION DOT Y,
		interface->widget_list->position.y );

	NTG_READ_FLOAT(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT POSITION DOT WIDTH,
		interface->widget_list->position.width );

	NTG_READ_FLOAT(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT POSITION DOT HEIGHT,
		interface->widget_list->position.height );

	NTG_READ_STRING(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT ATTRIBUTE_MAPPINGS DOT ATTRIBUTE_MAPPING DOT WIDGET_ATTRIBUTE,
		interface->widget_list->mapping_list->widget_attribute );

	NTG_READ_STRING(
		INTERFACE DOT WIDGET_INFO DOT WIDGET DOT ATTRIBUTE_MAPPINGS DOT ATTRIBUTE_MAPPING DOT ENDPOINT,
		interface->widget_list->mapping_list->endpoint );

	/* implementation info */

	NTG_READ_STRING( 
		INTERFACE DOT IMPLEMENTATION_INFO DOT PATCH_NAME,
		interface->implementation->patch_name );

	NTG_TRACE_ERROR_WITH_STRING( "unhandled element", element );
	
	return NTG_NO_ERROR;
}


ntg_error_code ntg_handle_interface_element_attributes( ntg_interface *interface, const char *element, xmlTextReaderPtr reader )
{
	char *module_guid_attribute = NULL;
	char *origin_guid_attribute = NULL;
	ntg_error_code error_code = NTG_NO_ERROR;

	assert( interface && element && reader );

	if( !strcmp( element, INTERFACE ) )
	{
		module_guid_attribute = ( char * ) xmlTextReaderGetAttribute( reader, NTG_ATTRIBUTE_MODULE_GUID );
		if( module_guid_attribute )
		{
			if( ntg_string_to_guid( module_guid_attribute, &interface->module_guid ) != NTG_NO_ERROR )
			{
				NTG_TRACE_ERROR_WITH_STRING( "Couldn't parse guid", module_guid_attribute );
				error_code = NTG_ERROR;
			}

			xmlFree( module_guid_attribute );
		}
		else
		{
			NTG_TRACE_ERROR( INTERFACE " lacks " NTG_ATTRIBUTE_MODULE_GUID " attribute" );
			error_code = NTG_ERROR;
		}

		origin_guid_attribute = ( char * ) xmlTextReaderGetAttribute( reader, NTG_ATTRIBUTE_ORIGIN_GUID );
		if( origin_guid_attribute )
		{
			if( ntg_string_to_guid( origin_guid_attribute, &interface->origin_guid ) != NTG_NO_ERROR )
			{
				NTG_TRACE_ERROR_WITH_STRING( "Couldn't parse guid", origin_guid_attribute );
				error_code = NTG_ERROR;
			}

			xmlFree( origin_guid_attribute );
		}
		else
		{
			NTG_TRACE_ERROR( INTERFACE " lacks " NTG_ATTRIBUTE_ORIGIN_GUID " attribute" );
			error_code = NTG_ERROR;
		}
	}

	return error_code;
}


ntg_interface *ntg_interface_load( const unsigned char *buffer, unsigned int buffer_size )
{
	xmlTextReaderPtr reader = NULL;
	const char *element_name = NULL;
	char *element_value = NULL; 
	char *inner_xml = NULL; 
	int depth, new_depth;
	bool handled_element_content = false;
	char *element_path = NULL;
	ntg_error_code error_code = NTG_FAILED;
	ntg_interface *interface = NULL;

	assert( buffer );

	interface = ntg_calloc( 1, sizeof( ntg_interface ) );

	xmlInitParser();

	reader = xmlReaderForMemory( (char *) buffer, buffer_size, NULL, NULL, 0 );
    if( !reader )
	{
		NTG_TRACE_ERROR( "unable to read file" );
		goto CLEANUP;
	}

	depth = -1;

	while( xmlTextReaderRead( reader ) )
	{
		switch( xmlTextReaderNodeType( reader ) )
		{
			case XML_READER_TYPE_TEXT:
				element_value = ( char * ) xmlTextReaderValue( reader );
				if( ntg_handle_interface_element_value( interface, element_path, element_value ) != NTG_NO_ERROR )
				{
					xmlFree( element_value );
					goto CLEANUP;
				}
				xmlFree( element_value );
				handled_element_content = true;
				break;

			case XML_READER_TYPE_ELEMENT:
				element_name = ( const char * ) xmlTextReaderConstName( reader );
				new_depth = xmlTextReaderDepth( reader );

				while( depth >= new_depth )
				{
					ntg_pop_element_name( element_path );
					depth--;
				}

				if( depth < new_depth )
				{
					element_path = ntg_push_element_name( element_path, element_name );
					depth++;
				}

				assert( depth == new_depth );

				if( ntg_handle_interface_element( interface, element_path ) != NTG_NO_ERROR )
				{
					goto CLEANUP;
				}

				if( xmlTextReaderHasAttributes( reader ) )
				{
					if( ntg_handle_interface_element_attributes( interface, element_path, reader ) != NTG_NO_ERROR )
					{
						goto CLEANUP;
					}
				}

				if( xmlTextReaderIsEmptyElement( reader ) )
				{
					ntg_handle_interface_element_value( interface, element_path, "" );
					handled_element_content = true;
				}
				else
				{
					handled_element_content = false;
					inner_xml = ( char * ) xmlTextReaderReadInnerXml( reader );
				}

				break;

			case XML_READER_TYPE_END_ELEMENT:
				if( !handled_element_content && inner_xml )
				{
					ntg_handle_interface_element_value( interface, element_path, inner_xml );
				}
				if( inner_xml )
				{
					xmlFree( inner_xml );
					inner_xml = NULL;
				}
				handled_element_content = true;
				
				break;

			default:
				break;
		}
	}

	ntg_reverse_lists( interface );

	error_code = ntg_sanity_check( interface );
	if( error_code != NTG_NO_ERROR )
	{
		NTG_TRACE_ERROR( "sanity check failed" );
	}

	ntg_propogate_defaults( interface );

	ntg_assign_endpoint_indices( interface );

CLEANUP:

	if( element_path )
	{
		ntg_free( element_path );
	}

	if( reader )
	{
		xmlFreeTextReader( reader );
	}

	if( error_code == NTG_NO_ERROR )
	{
		NTG_TRACE_VERBOSE_WITH_STRING( "Loaded ok", interface->info->name );
	}
	else
	{
		ntg_interface_free( interface );
		interface = NULL;
	}

	return interface;
}


bool ntg_interface_is_core( const ntg_interface *interface )
{
	ntg_tag *tag_iterator;

	assert( interface );

	for( tag_iterator = interface->info->tag_list; tag_iterator; tag_iterator = tag_iterator->next )
	{
		if( strcmp( tag_iterator->tag, NTG_CORE_TAG ) == 0 )
		{
			return true;
		}
	}

	return false;
}


bool ntg_interface_is_core_name_match( const ntg_interface *interface, const char *name )
{
	if( ntg_interface_is_core( interface ) ) 
	{
		return( strcmp( interface->info->name, name ) == 0 );
	}
	else
	{
		return false;
	}
}


bool ntg_interface_has_implementation( const ntg_interface *interface )
{
	if( interface->implementation  )
	{
		if( interface->implementation->patch_name )
		{
			return true;
		}
	}

	return false;
}


bool ntg_interface_should_embed_module( const ntg_interface *interface )
{
	return !interface->info->implemented_in_libintegra;
}


bool ntg_endpoint_is_input_file( const ntg_endpoint *endpoint )
{
	assert( endpoint );

	if( !endpoint->control_info ) return false;
	if( !endpoint->control_info->state_info ) return false;

	return endpoint->control_info->state_info->is_input_file;
}


bool ntg_endpoint_should_send_to_host( const ntg_endpoint *endpoint )
{
    assert( endpoint );

	if( endpoint->type != NTG_CONTROL ) return false;

	return endpoint->control_info->is_sent_to_host;
}


bool ntg_endpoint_should_load_from_ixd( const ntg_endpoint *endpoint, ntg_value_type loaded_type )
{
	/* 
	Just because a value is in an ixd file, doesn't mean we want to load it.  
	The interface could've changed since ixd was written.
	*/

	assert( endpoint );

	if( endpoint->type != NTG_CONTROL ) return false;
	if( endpoint->control_info->type != NTG_STATE ) return false;
	if( !endpoint->control_info->state_info->is_saved_to_file ) return false;
	if( endpoint->control_info->state_info->type != loaded_type ) return false;

	return true;
}


bool ntg_endpoint_is_audio_stream( const ntg_endpoint *endpoint )
{
	if( endpoint->type != NTG_STREAM ) return false;

	return ( endpoint->stream_info->type == NTG_AUDIO_STREAM );
}



