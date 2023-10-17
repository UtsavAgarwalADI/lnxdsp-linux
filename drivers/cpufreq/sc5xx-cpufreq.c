/*
 * CPU Frequency driver for sc5xx boards
 *
 * Copyright (c) 2023 Analog Devices Inc. 
 *
 * */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/types.h>
//#include <linux/platform.h>

#define CLK "arm" 

/*
 * All processor frequency definitions
 * */
#if defined(CONFIG_ARCH_SC58X) || defined(CONFIG_ARCH_SC59X) || defined(CONFIG_ARCH_SC59X_64)
#define MIN_MHZ 800
#define MAX_MHZ 1000
#else
#define MIN_MHZ 0
#define MAX_MHZ 0
#endif

/*
 * Processor Transition Latency
 * */
#define TRANSITION_LATENCY_NS 50000

static int sc5xx_verify(struct cpufreq_policy_data *policy) {
	unsigned int min, max;
	
	min = MIN_MHZ * 1000;
	min = MAX_MHZ * 1000;
	if((!min)&&(!max))
		return -ENODEV; 

	cpufreq_verify_within_limits(policy, min, max);
	return 0;
}

/*
 * Since the final clk for the ARM processor is via the CLK clk, it is agnostic 
 * to what archetecture it is, since different clocks are multiplexed to the 
 * parent before it.
 * */
static unsigned int sc5xx_get_cpu_freq(unsigned int) {
	struct clk *core_clk;
	unsigned int core_clk_rate;

	core_clk = clk_get(NULL,CLK); 
	
	if(IS_ERR(core_clk)) {
		printk(KERN_INFO"could not get clk:%s\n",CLK);
		return 0;
	}
	
	printk(KERN_INFO"Successfully obtained processor clock speed:%u\n",core_clk_rate);
	core_clk_rate = clk_get_rate(core_clk);
	return core_clk_rate;

}

/*
 * .driver data will contain the number of table entries
 * remaining after the current entry. This allows keeping 
 * track of the size of the table without using a separate
 * data structure.
 *
 * The table should be ended with a CPUFREQ_TABLE_END entry,
 * with driver data as -1
 *
 * For instance if there are 3 entries, the .driver_data 
 * field should be populated as: 
 * {
 * 	{
 * 		...
 * 		.driver_data = 2,
 * 	},
 * 	{
 * 		...
 * 		.driver_data = 1,
 * 	},
 * 	{
 * 		...
 * 		.driver_data = 0,
 * 	},
 * 	{
 * 		.frequency = CPUFREQ_TABLE_END,
 * 		.driver_data = -1,
 * 	},
 * };
 * 
 *
 * The default driver contains a min and max entry, but this can be 
 * customized as per requirements.
 * * */
struct cpufreq_frequency_table sc5xx_frequency_table[] = {
	{
		.frequency = MAX_MHZ * 1000,
		.driver_data = 1,
	},
	{
		.frequency = MIN_MHZ * 1000,
		.driver_data = 0,
	},
	{
		.frequency = CPUFREQ_TABLE_END,
		.driver_data = -1,
	},

};

static int set_sc5xx_cpu_freq(unsigned int new_freq) {
	struct clk *core_clk;
	int err;

	core_clk = clk_get(NULL,CLK); 
	
	if(IS_ERR(core_clk))
		return -ENODEV;
	
	err = clk_set_rate(core_clk, new_freq);
	clk_put(core_clk);
	return err;
}

static int sc5xx_target_index(struct cpufreq_policy *policy, unsigned int index) {
	unsigned int max_freq_nodes, curr_freq, new_freq;
	int err = 0;

	max_freq_nodes = sc5xx_frequency_table[0].driver_data;
	if(max_freq_nodes < index) {
		printk(KERN_WARNING"Invalid frequency index provided\n");
		return 0;
	}

	new_freq = sc5xx_frequency_table[index].frequency;
	curr_freq = sc5xx_get_cpu_freq(0); 
	
	if((!curr_freq)&&(!new_freq))
		return -ENODEV;
	
	/*
	 * If the function returns an error, the new clock
	 * rate has not been set by default and no further 
	 * action should be necessary
	 * */
	err = set_sc5xx_cpu_freq(new_freq);
	if(IS_ERR_VALUE(err)) 
		return err;
	
	return 0;
}

static int sc5xx_init(struct cpufreq_policy *policy) {
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY_NS;
	policy->freq_table = sc5xx_frequency_table;
	return 0;
}

static struct cpufreq_driver sc5xx_driver = {
	.name = "adsp-sc5xx cpufreq",
	.init = sc5xx_init,
	.verify = sc5xx_verify,
	.target_index = sc5xx_target_index,
	.get = sc5xx_get_cpu_freq,
	.attr = cpufreq_generic_attr,
};

/* TO BE REPLACED BY PLATFORM DRIVER */
static int __init load_cpufreq(void) {
	int err;
	
	//debug
	int i;
	for(i=0; i<=sc5xx_frequency_table[0].driver_data;i++){
		printk(KERN_ALERT"DEBUG:Freq table[%d]:freq:[%u]Hz\n",i,sc5xx_frequency_table[i].frequency);
	}

	err = cpufreq_register_driver(&sc5xx_driver);
	if (IS_ERR_VALUE(err))
		return err;

	printk("Loaded cpufreq driver for sc5xx!\n");
	return 0;
}

static void __exit unload_cpufreq(void)
{
	cpufreq_unregister_driver(&sc5xx_driver);
	printk("Unloaded cpufreq driver for sc5xx\n");
}

MODULE_AUTHOR("Utsav Agarwal <utsav.agarwal@analog.com>");
MODULE_DESCRIPTION("cpufreq driver for ADSP-SC5XX boards");
MODULE_LICENSE("GPL");


module_init(load_cpufreq);
module_exit(unload_cpufreq);
