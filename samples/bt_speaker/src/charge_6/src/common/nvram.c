#include "common/nvram.h"
#include "property_manager.h"

int property_factory_reset(void)
{
    //todo: only is clear user info ?
    property_clear_user_info();

    return 0;
}
