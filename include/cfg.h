#ifndef __CFG_H__
#define __CFG_H__

#include <libconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

	// Opens a configuration from path and returns it
	// Returns NULL on failure
	config_t *cfg_open(const char *path);

	// Closes a configuration
	void cfg_close(config_t *config);

	// Saves a configuration to path
	void cfg_save(char *path, config_t *config);

	// Get values from a setting path
	// Returns initial value if setting isn't present
	unsigned char config_get_bool(config_t *config, char *setting, unsigned char initial);
	unsigned int config_get_uint(config_t *config, char *setting, unsigned int initial);
	int config_get_int(config_t *config, char *setting, int initial);
	int config_get_int_elem(config_t *config, char *setting, int element, int initial);
	const char *config_get_string(config_t *config, char *setting, const char *initial);
	const char *config_get_string_elem(config_t *config, char *setting, int element, const char *initial);

	void cfg_int_to_string(char *out, int in);
	long cfg_string_to_int(const char *in);

	#define cfg_lookup(A)           config_lookup(config,A)
	#define cfg_get_bool(A,B)       config_get_bool(config,A,B)
	#define cfg_get_uint(A,B)       config_get_uint(config,A,B)
	#define cfg_get_int(A,B)        config_get_int(config,A,B)
	#define cfg_get_int_elem(A,B,C) config_get_int_elem(config,A,B,C)
	#define cfg_get_string(A,B)     config_get_string(config,A,B)
	#define cfg_get_string_elem(A,B,C) config_get_string_elem(config,A,B,C)

#ifdef __cplusplus
};
#endif

#endif /*__CFG_H__*/
