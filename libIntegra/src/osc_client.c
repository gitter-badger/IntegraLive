#include "platform_specifics.h"

#include "osc_client.h"
#include "command.h"
#include "helper.h"
#include "memory.h"
#include "globals.h"
#include "server.h"

#include <assert.h>


static const char *ntg_command_source_text[NTG_COMMAND_SOURCE_end] =  {
    "initialization",
	"load",
	"system",
	"connection",
    "host",
    "script",
    "xmlrpc_api",
    "osc_api",
    "c_api" };



/* sure there must be a more elegant way to do this, but lo_message_add_varargs
 * doesn't seem to work, and this does */
ntg_error_code ntg_osc_send_ssss(lo_address targ, const char *path, 
        const char *s1, const char *s2, const char *s3, const char *s4)
{
    lo_send(targ, path, "ssss", s1, s2, s3, s4);

	return NTG_NO_ERROR;
}

ntg_error_code ntg_osc_send_sss(lo_address targ, const char *path, 
        const char *s1, const char *s2, const char *s3)
{
    lo_send(targ, path, "sss", s1, s2, s3);

	return NTG_NO_ERROR;
}

ntg_error_code ntg_osc_send_ssi(lo_address targ, const char *path, 
        const char *s1, const char *s2, int i)
{
    lo_send(targ, path, "ssi", s1, s2, i);

	return NTG_NO_ERROR;
}

ntg_error_code ntg_osc_send_ssf(lo_address targ, const char *path, 
        const char *s1, const char *s2, float f)
{
    lo_send(targ, path, "ssf", s1, s2, f);

	return NTG_NO_ERROR;
}

ntg_error_code ntg_osc_send_ssN(lo_address targ, const char *path, 
        const char *s1, const char *s2)
{
    lo_send(targ, path, "ssN", s1, s2);

	return NTG_NO_ERROR;
}

ntg_error_code ntg_osc_send_ss(lo_address targ, const char *path, 
        const char *s1, const char *s2)
{
    lo_send(targ, path, "ss", s1, s2);

	return NTG_NO_ERROR;
}


ntg_osc_client *ntg_osc_client_new(const char *url, unsigned short port)
{
    char port_string[6];
	ntg_osc_client *client = NULL;

    port_string[5]=0;
    snprintf(port_string, 5, "%d", port);

    client = ntg_malloc(sizeof(ntg_osc_client));
    client->address = lo_address_new(url, port_string);

    if(client->address == NULL) {
        assert (false);
    }

    return client;
}

void ntg_osc_client_destroy(ntg_osc_client *client) 
{
	assert( client );
	assert( client->address );

    lo_address_free(client->address);
    ntg_free(client);
}

ntg_error_code ntg_osc_client_send_set(ntg_osc_client *client,
        ntg_command_source cmd_source,
        const ntg_path *path,
        const ntg_value *value)
{
	const char *methodName = "/command.set";
	const char *cmd_source_s = NULL;
    int value_i   = 0;
    float value_f = 0.f;
    char *value_s = NULL;

    assert(client != NULL);
    assert(path != NULL);

    cmd_source_s = ntg_command_source_text[cmd_source];

	if( value )
	{
		switch( ntg_value_get_type(value) ) 
		{
			case NTG_INTEGER:
				value_i = ntg_value_get_int(value);
				ntg_osc_send_ssi(client->address, methodName, cmd_source_s,
						path->string, value_i);
				break;
			case NTG_FLOAT:
				value_f = ntg_value_get_float(value);
				ntg_osc_send_ssf(client->address, methodName, cmd_source_s,
						path->string, value_f);
				break;
			case NTG_STRING:
				value_s = ntg_value_get_string(value);
				ntg_osc_send_sss(client->address, methodName, cmd_source_s,
						path->string, value_s);
				break;

			default:
				assert( false );
				break;
		}
	}
	else
	{
		ntg_osc_send_ssN(client->address, methodName, cmd_source_s,
						path->string);
	}

    return NTG_NO_ERROR;
}

ntg_error_code ntg_osc_client_send_new(ntg_osc_client *client,
        ntg_command_source cmd_source,
        const GUID *module_id,
        const char *node_name,
        const ntg_path *path)
{
    const char *methodName = "/command.new";
    const char *cmd_source_s = NULL;
	char *module_id_string;

	assert(client != NULL);
    assert(module_id != NULL);
    assert(node_name != NULL);
    assert(path != NULL);
    assert(path->string != NULL);

    cmd_source_s = ntg_command_source_text[cmd_source];

	module_id_string = ntg_guid_to_string( module_id );

    ntg_osc_send_ssss(client->address, methodName, cmd_source_s,
            module_id_string, node_name, path->string);

	ntg_free( module_id_string );

    return 0;

}

ntg_error_code ntg_osc_client_send_load(ntg_osc_client *client,
        ntg_command_source cmd_source,
        const char *file_path,
        const ntg_path *path)
{
    char *const methodName = "/command.load";
    const char *cmd_source_s = ntg_command_source_text[cmd_source];

    assert(client != NULL);
    assert(file_path != NULL);
    assert(path != NULL);

    return ntg_osc_send_sss(client->address, methodName, cmd_source_s,
            file_path, path->string);
}

ntg_error_code ntg_osc_client_send_delete(ntg_osc_client *client,
        ntg_command_source cmd_source,
        const ntg_path *path)
{
    char *const methodName = "/command.delete";
    const char *cmd_source_s = ntg_command_source_text[cmd_source];

    assert(client != NULL);
    assert(path != NULL);

    return ntg_osc_send_ss(client->address, methodName, cmd_source_s,
            path->string);
}
   
ntg_error_code ntg_osc_client_send_move(ntg_osc_client *client,
        ntg_command_source cmd_source,
        const ntg_path *node_path,
        const ntg_path *parent_path)
{
    char *const methodName = "/command.move";
    const char *cmd_source_s = ntg_command_source_text[cmd_source];

    assert(client != NULL);
    assert(node_path != NULL);
    assert(parent_path != NULL);

	return ntg_osc_send_sss(client->address, methodName, cmd_source_s,
            node_path->string, parent_path->string);

}

ntg_error_code ntg_osc_client_send_rename(ntg_osc_client *client,
        ntg_command_source cmd_source,
        const ntg_path *path,
        const char *name)
{
    char *const methodName = "/command.rename";
    const char *cmd_source_s = ntg_command_source_text[cmd_source];

    assert(client != NULL);
    assert(path != NULL);
    assert(name != NULL);

    return ntg_osc_send_sss(client->address, methodName, cmd_source_s,
            path->string, name);

}



