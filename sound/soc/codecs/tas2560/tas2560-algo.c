#include <sound/smart_amp.h>
#include <sound/q6afe-v2.h>
#include <tas2560-calib.h>

/*Master Control to Bypass the Smartamp TI CAPIv2 module*/
static int smartamp_bypass = TAS_FALSE;
static int smartamp_enable = TAS_FALSE;
static int calib_state = 0;
static int calib_re = 0;
static struct mutex routing_lock;
int smartamp_get_val(int value)
{
	return ((value == TAS_TRUE)?TRUE:FALSE);
}

int smartamp_set_val(int value)
{
	return ((value == TRUE)?TAS_TRUE:TAS_FALSE);
}

int afe_smartamp_get_set(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id)
{
	int ret = 0;
	struct afe_smartamp_get_calib calib_resp;

	switch (get_set) {
		case TAS_SET_PARAM:
			if(smartamp_get_val(smartamp_bypass))
			{
				pr_err("TI-SmartPA: %s: SmartAmp is bypassed no control set", __func__);
				goto fail_cmd;
			}
			ret = afe_smartamp_set_calib_data(param_id, 
				(struct afe_smartamp_set_params_t *)user_data, length, module_id);
		break;	
		case TAS_GET_PARAM:
			memset(&calib_resp, 0, sizeof(calib_resp));
			ret = afe_smartamp_get_calib_data(&calib_resp,
				param_id, module_id);
			memcpy(user_data, calib_resp.res_cfg.payload, length);
		break;
		default:
			goto fail_cmd;
	}
fail_cmd:
	return ret;
}

/*Wrapper arround set/get parameter, all set/get commands pass through this wrapper*/
int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id)
{
	int ret = 0;
	mutex_lock(&routing_lock);
	ret = afe_smartamp_get_set(user_data, param_id, get_set, length, module_id);
	mutex_unlock(&routing_lock);
	return ret;
}

/*Control-1: Set Profile*/
static const char *profile_index_text[] = {"NONE","MUSIC","RING","VOICE","CALIB"};
static const struct soc_enum profile_index_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(profile_index_text), profile_index_text),
};

static int tas2560_set_profile(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = pUcontrol->value.integer.value[0];
	int param_id = 0;		

	pr_info("TI-SmartPA: %s: Setting profile %s", __func__, profile_index_text[user_data]);
	//To exclude None
	if(user_data)
		user_data -= 1;
	else
		return 0;	

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_PROFILE, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("TI-SmartPA: %s: Failed to set profile", __func__);
		return -1;
	}
	if(smartamp_get_val(smartamp_enable))
	{
		pr_info("TI-SmartPA: %s: Sending RX Config", __func__);
		param_id = CAPI_V2_TAS_RX_CFG;
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
			pr_err("TI-SmartPA: %s: Failed to set config", __func__);
	}
	return ret;
}

static int tas2560_get_profile(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;	

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_PROFILE, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("TI-SmartPA: %s: Failed to get profile", __func__);
			user_data = 0;
		}
		else
			user_data += 1;
	}	

	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: getting profile %s", __func__, profile_index_text[user_data]);

	return 0;
}

/*Control-2: Set Calibrated Rdc*/
static int tas2560_set_Re_left(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = pUcontrol->value.integer.value[0];
	int param_id = 0;
	
	calib_re = user_data;
	pr_info("TI-SmartPA: %s: Setting Re %d", __func__, user_data);
	
	param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_RE, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("TI-SmartPA: %s: Failed to set Re", __func__);
		return -1;
	}

	return 0;
}

static int tas2560_dummy_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = calib_re;
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: write Only %d", __func__, user_data);
	return ret;
}

/*Control-3: Calibration and Test(F0,Q,Tv) Controls*/
static const char *tas2560_calib_test_text[] = {
	"NONE",
	"CALIB_START",
	"CALIB_STOP",
	"TEST_START",
	"TEST_STOP"
};

static const struct soc_enum tas2560_calib_test_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_calib_test_text), tas2560_calib_test_text),
};

static int tas2560_calib_test_set_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int param_id = 0;
	int user_data = pUcontrol->value.integer.value[0];
	int data = 1;

	calib_state = user_data;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_calib_test_text[user_data]);

	switch(user_data) {
		case CALIB_START:
			param_id = TAS_CALC_PARAM_IDX(TAS_SA_CALIB_INIT, 1, CHANNEL0);		
		break;
		case CALIB_STOP:
			param_id = TAS_CALC_PARAM_IDX(TAS_SA_CALIB_DEINIT, 1, CHANNEL0);
		break;
		case TEST_START:
			pr_info("TI-SmartPA: %s: Not Required!", __func__);
		break;
		case TEST_STOP:
			pr_info("TI-SmartPA: %s: Not Required!", __func__);
		break;
		default:
			pr_info("TI-SmartPA: %s: Normal", __func__);
		break;		
	}
	
	if(param_id)
	{
		ret = afe_smartamp_algo_ctrl((u8 *)&data, param_id,
			TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("TI-SmartPA: %s: Failed to set calib/test", __func__);
			return 1;
		}
	}

	return ret;			
}

static int tas2560_calib_test_get_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	pUcontrol->value.integer.value[0] = calib_state;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_calib_test_text[calib_state]);
	return ret;
}

/*Control-4: Get Re*/
static int tas2560_get_re_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;	

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_RE, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("TI-SmartPA: %s: Failed to get Re", __func__);
			return -1;
		}
	}
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: Getting Re %d", __func__, user_data);

	return 0;
}

/*Control-5: Get F0*/
static int tas2560_get_f0_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;	

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_F0, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("TI-SmartPA: %s: Failed to get F0", __func__);
			return -1;
		}
	}	
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: Getting F0 %d", __func__, user_data);

	return 0;
}

/*Control-6: Get Q*/
static int tas2560_get_q_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;	

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_Q, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("TI-SmartPA: %s: Failed to get q", __func__);
			return -1;
		}
	}	
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: Getting q %d", __func__, user_data);

	return 0;
}

/*Control-7: Get Tv*/
static int tas2560_get_tv_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;	

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_TV, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("TI-SmartPA: %s: Failed to get Tv", __func__);
			return -1;
		}
	}	
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: Getting Tv %d", __func__, user_data);

	return 0;
}

/*Control-8: Smartamp Enable*/
static const char *tas2560_smartamp_enable_text[] = {
	"DISABLE",
	"ENABLE"
};

static const struct soc_enum tas2560_smartamp_enable_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_smartamp_enable_text), tas2560_smartamp_enable_text),
};
static int tas2560_smartamp_enable_set(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int param_id = 0;
	int user_data = pUcontrol->value.integer.value[0];

	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_enable_text[user_data]);

	smartamp_enable = smartamp_set_val(user_data);
	if(smartamp_get_val(smartamp_enable) == FALSE)
	{
		pr_info("TI-SmartPA: %s: Disable called", __func__);
		calib_state = 0;
		calib_re = 0;
		return 0;
	}
	pr_info("TI-SmartPA: %s: Setting the feedback module info for TAS", __func__);		
	ret = afe_spk_prot_feed_back_cfg(TAS_TX_PORT,TAS_RX_PORT, 1, 0, 1);
	if(ret)
		pr_err("TI-SmartPA: %s: FB Path Info failed ignoring ret = 0x%x", __func__, ret);

	pr_info("TI-SmartPA: %s: Sending TX Enable", __func__);
	param_id = CAPI_V2_TAS_TX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_TX);
	if(ret)
	{
		pr_err("TI-SmartPA: %s: TX Enable Failed ret = 0x%x", __func__, ret);
		goto fail_cmd;
	}
	
	pr_info("TI-SmartPA: %s: Sending RX Enable", __func__);
	param_id = CAPI_V2_TAS_RX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret)
	{
		pr_err("TI-SmartPA: %s: RX Enable Failed ret = 0x%x", __func__, ret);
		goto fail_cmd;	
	}

	pr_info("TI-SmartPA: %s: Sending RX Config", __func__);
	param_id = CAPI_V2_TAS_RX_CFG;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
		pr_err("TI-SmartPA: %s: Failed to set config", __func__);
fail_cmd:		
	return ret;
}

static int tas2560_smartamp_enable_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = smartamp_get_val(smartamp_enable);
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_enable_text[user_data]);
	return ret;
}

/*Control-9: Smartamp Bypass */
static const char *tas2560_smartamp_bypass_text[] = {
	"FALSE",
	"TRUE"
};

static const struct soc_enum tas2560_smartamp_bypass_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_smartamp_bypass_text), tas2560_smartamp_bypass_text),
};

static int tas2560_smartamp_bypass_set(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = pUcontrol->value.integer.value[0];
	if(smartamp_get_val(smartamp_enable))
	{
		pr_debug("TI-SmartPA: %s: cannot update while smartamp enabled", __func__);
		return ret;
	}	
	smartamp_bypass = smartamp_set_val(user_data);
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_bypass_text[user_data]);
	return ret;
}

static int tas2560_smartamp_bypass_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = smartamp_get_val(smartamp_bypass);
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_bypass_text[user_data]);
	return ret;
}

/*Control-10: Smartamp Status*/
static const char *tas2560_smartamp_status_text[] = {
	"DISABLED",
	"ENABLED"
};

static const struct soc_enum tas2560_smartamp_status_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_smartamp_status_text), tas2560_smartamp_status_text),
};

static int tas2560_get_smartamp_status(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;
	int data = 1;	

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_STATUS, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("TI-SmartPA: %s: Failed to get Status", __func__);
		user_data = 0;
	}
	else
		pr_info("TI-SmartPA: %s: status = %d", __func__, user_data);
	data &= user_data;
	user_data = 0;
	param_id = CAPI_V2_TAS_RX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("TI-SmartPA: %s: Failed to get Rx Enable", __func__);
		user_data = 0;
	}else
		pr_info("TI-SmartPA: %s: Rx Enable = %d", __func__, user_data);

	data &= user_data;
	user_data = 0;
	param_id = CAPI_V2_TAS_TX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_TX);
	if(ret < 0)
	{
		pr_err("TI-SmartPA: %s: Failed to get Tx Enable", __func__);
		user_data = 0;
	}else
		pr_info("TI-SmartPA: %s: Tx Enable = %d", __func__, user_data);

	data &= user_data;	
	pUcontrol->value.integer.value[0] = data;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_status_text[data]);

	return 0;
}

static const struct snd_kcontrol_new smartamp_tas2560_mixer_controls[] = {
	SOC_ENUM_EXT("TAS2560_ALGO_PROFILE", profile_index_enum[0],
		tas2560_get_profile, tas2560_set_profile),
	SOC_SINGLE_EXT("TAS2560_SET_RE_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,			
	        tas2560_dummy_get, tas2560_set_Re_left),
	SOC_ENUM_EXT("TAS2560_ALGO_CALIB_TEST", tas2560_calib_test_enum[0],
		tas2560_calib_test_get_left, tas2560_calib_test_set_left),
	SOC_SINGLE_EXT("TAS2560_GET_RE_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,			
	        tas2560_get_re_left, NULL),
	SOC_SINGLE_EXT("TAS2560_GET_F0_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,			
	        tas2560_get_f0_left, NULL),
	SOC_SINGLE_EXT("TAS2560_GET_Q_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,			
	        tas2560_get_q_left, NULL),
	SOC_SINGLE_EXT("TAS2560_GET_TV_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,			
	        tas2560_get_tv_left, NULL),
	SOC_ENUM_EXT("TAS2560_SMARTPA_ENABLE", tas2560_smartamp_enable_enum[0],			
	        tas2560_smartamp_enable_get, tas2560_smartamp_enable_set),
	SOC_ENUM_EXT("TAS2560_ALGO_BYPASS", tas2560_smartamp_bypass_enum[0],
		tas2560_smartamp_bypass_get, tas2560_smartamp_bypass_set),
	SOC_ENUM_EXT("TAS2560_ALGO_STATUS", tas2560_smartamp_status_enum[0],
		tas2560_get_smartamp_status, NULL),
};

void codec_smartamp_add_controls(struct snd_soc_codec *codec)
{
	pr_err("TI-SmartPA: %s: Adding smartamp controls", __func__);
	mutex_init(&routing_lock);
	snd_soc_add_codec_controls(codec, smartamp_tas2560_mixer_controls,
                                ARRAY_SIZE(smartamp_tas2560_mixer_controls));
	tas_calib_init();
	
}

void codec_smartamp_remove_controls(struct snd_soc_codec *codec)
{
	(void)codec;
	tas_calib_exit();
	//TODO: Mutex Deinit ??
}