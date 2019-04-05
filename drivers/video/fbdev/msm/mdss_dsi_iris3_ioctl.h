#ifndef MDSS_DSI_IRIS_IOCTL
#define MDSS_DSI_IRIS_IOCTL

int msmfb_iris_operate_tool(struct msm_fb_data_type *mfd,
				void __user *argp);

int msmfb_iris_operate_conf(struct msm_fb_data_type *mfd,
				void __user *argp);
#endif
