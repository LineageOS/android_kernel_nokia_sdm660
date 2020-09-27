/*
 * fih_msm_actuator_recover.h
 *
 * init setting refer to actuator header
 * ex : ak7371_wide_b2n_actuator.h (mm-camera\mm-camera2\media-controller\modules\sensors\actuator\libs\ak7371_wide_b2n)
 *
 * enum value refer to below header
 * - sensor_sdk_common.h (mm-camerasdk\sensor\includes)
 * - actuator_driver.h (mm-camerasdk\sensor\includes)
 *
 */

struct actuator_out_info {
    char name[MAX_SENSOR_NAME];
    struct reg_settings_t init_settings[16];
    unsigned int init_setting_size;
};

struct actuator_out_info ak7371_wide_b2n_actuator =
{
    .name="ak7371_wide_b2n",
    .init_settings =
    {
        {
            .reg_addr = 0x02,
            .addr_type = 1,//CAMERA_I2C_BYTE_ADDR,
            .reg_data = 0x00,/*Set to Active mode*/
            .data_type = 1,//CAMERA_I2C_BYTE_DATA,
            .i2c_operation = 0,//ACTUATOR_I2C_OP_WRITE,
            .delay = 0,
        },
    },
    .init_setting_size = 1,
};

struct actuator_out_info ak7371_tele_b2n_actuator =
{
    .name="ak7371_tele_b2n",
    .init_settings =
    {
        {
            .reg_addr = 0x02,
            .addr_type = 1,//CAMERA_I2C_BYTE_ADDR,
            .reg_data = 0x00,/*Set to Active mode*/
            .data_type = 1,//CAMERA_I2C_BYTE_DATA,
            .i2c_operation = 0,//ACTUATOR_I2C_OP_WRITE,
            .delay = 0,
        },
    },
    .init_setting_size = 1,
};

int fih_get_recover_actuator_setting(char *actuator_name, struct reg_settings_t **actuator_setting, int *setting_size)
{
    if(strncmp(actuator_name, ak7371_wide_b2n_actuator.name, strlen(ak7371_wide_b2n_actuator.name))==0)
    {
        *actuator_setting = ak7371_wide_b2n_actuator.init_settings;
        *setting_size = ak7371_wide_b2n_actuator.init_setting_size;
    }
    else if(strncmp(actuator_name, ak7371_tele_b2n_actuator.name, strlen(ak7371_tele_b2n_actuator.name))==0)
    {
        *actuator_setting = ak7371_tele_b2n_actuator.init_settings;
        *setting_size = ak7371_tele_b2n_actuator.init_setting_size;
    }else
        return -1;

    return 0;
}