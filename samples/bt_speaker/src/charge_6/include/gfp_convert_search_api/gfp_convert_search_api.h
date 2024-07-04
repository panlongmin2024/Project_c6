#ifndef __GFP_CONVERT_SEARCH_API__
#define __GFP_CONVERT_SEARCH_API__

#include <zephyr.h>

struct gfp_model_id_convert_search_table_element_type {
    u8_t model_id[3];
    char *private_key;
    u8_t private_key_base64[32];
};

const struct gfp_model_id_convert_search_table_element_type *get_gfp_model_id_convert_search_table(void);

int get_gfp_model_id_convert_search_table_array_size(void);

const u8_t *gfp_model_id_convert_to_private_key_base64(const u8_t *model_id);
int gfp_model_id_convert_to_index(const u8_t *model_id);

#endif
