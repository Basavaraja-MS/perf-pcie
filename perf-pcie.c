#include <linux/cpumask.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
//#include <linux/cpuhotplug.h>

#include <linux/perf_event.h>

//#include "cpuhotplug.h"

MODULE_LICENSE("Dual BSD/GPL");


#define MMDC_FLAG_PROFILE_SEL	0x1
#define MMDC_PRF_AXI_ID_CLEAR	0x0

#define PCIE_NUM_COUNTERS	7
#define HRTIMER
#define CPUHP_AP_ONLINE_DYN	135

static enum cpuhp_state cpuhp_pcie_state;
static DEFINE_IDA(pcie_ida);

struct fsl_pcie_devtype_data {
	unsigned int flags;
};

static const struct fsl_pcie_devtype_data imx6q_data = {
};

static const struct fsl_pcie_devtype_data imx6qp_data = {
	.flags = MMDC_FLAG_PROFILE_SEL,
};

static const struct of_device_id imx_pcie_dt_ids[] = {
	{ .compatible = "fsl,imx6q-pcie", .data = (void *)&imx6q_data},
	{ .compatible = "fsl,imx6qp-pcie", .data = (void *)&imx6qp_data},
	{ /* sentinel */ }
};

/*
PMU_EVENT_ATTR_STRING(total-cycles, pcie_pmu_total_cycles, "event=0x00")
PMU_EVENT_ATTR_STRING(busy-cycles, pcie_pmu_busy_cycles, "event=0x01")
PMU_EVENT_ATTR_STRING(read-accesses, pcie_pmu_read_accesses, "event=0x02")
PMU_EVENT_ATTR_STRING(write-accesses, pcie_pmu_write_accesses, "event=0x03")
PMU_EVENT_ATTR_STRING(read-bytes, pcie_pmu_read_bytes, "event=0x04")
PMU_EVENT_ATTR_STRING(read-bytes.unit, pcie_pmu_read_bytes_unit, "MB");
PMU_EVENT_ATTR_STRING(read-bytes.scale, pcie_pmu_read_bytes_scale, "0.000001");
PMU_EVENT_ATTR_STRING(write-bytes, pcie_pmu_write_bytes, "event=0x05")
PMU_EVENT_ATTR_STRING(write-bytes.unit, pcie_pmu_write_bytes_unit, "MB");
PMU_EVENT_ATTR_STRING(write-bytes.scale, pcie_pmu_write_bytes_scale, "0.000001");
*/

PMU_EVENT_ATTR_STRING(TxTLPcount, pcie_pmu_TxTLPcount, "event=0x00")
PMU_EVENT_ATTR_STRING(TxTLPpayloadCount, pcie_pmu_TxTLPpayloadCount, "event=0x01")
PMU_EVENT_ATTR_STRING(RxTLPcount, pcie_pmu_RxTLPcount, "event=0x02")
PMU_EVENT_ATTR_STRING(RxTLPpayloadCount, pcie_pmu_RxTLPpayloadCount, "event=0x03")
PMU_EVENT_ATTR_STRING(LCRCErrorCount, pcie_pmu_LCRCErrorCount, "event=0x04")
PMU_EVENT_ATTR_STRING(ECCcorrectableErrorCount, pcie_pmu_ECCcorrectableErrorCount, "event=0x05")
PMU_EVENT_ATTR_STRING(ECCcorrectableErrorCountRegisterforAXIRAMs, pcie_pmu_ECCcorrectableErrorCountRegisterforAXIRAMs, "event=0x06")

struct cpumask file;

/*
static struct attribute *pcie_pmu_events_attrs[] = {
        &pcie_pmu_total_cycles.attr.attr,
        &pcie_pmu_busy_cycles.attr.attr,
        &pcie_pmu_read_accesses.attr.attr,
        &pcie_pmu_write_accesses.attr.attr,
        &pcie_pmu_read_bytes.attr.attr,
        &pcie_pmu_read_bytes_unit.attr.attr,
        &pcie_pmu_read_bytes_scale.attr.attr,
        &pcie_pmu_write_bytes.attr.attr,
        &pcie_pmu_write_bytes_unit.attr.attr,
        &pcie_pmu_write_bytes_scale.attr.attr,

	&pcie_pmu_TxTLPcount.attr.attr,
        NULL,
};
*/

static struct attribute *pcie_pmu_events_attrs[] = {
        
	&pcie_pmu_TxTLPcount.attr.attr,
	&pcie_pmu_TxTLPpayloadCount.attr.attr,
	&pcie_pmu_RxTLPcount.attr.attr,
	&pcie_pmu_RxTLPpayloadCount.attr.attr,
	&pcie_pmu_LCRCErrorCount.attr.attr,
	&pcie_pmu_ECCcorrectableErrorCount.attr.attr,
	&pcie_pmu_ECCcorrectableErrorCountRegisterforAXIRAMs.attr.attr,
        NULL,
};

/*
static int bitmap_print_to_pagebuf(bool list, char *buf, const unsigned long *maskp,
			    int nmaskbits)
{
	ptrdiff_t len = PTR_ALIGN(buf + PAGE_SIZE - 1, PAGE_SIZE) - buf;
	int n = 0;

	if (len > 1)
		n = list ? scnprintf(buf, len, "%*pbl\n", nmaskbits, maskp) :
			   scnprintf(buf, len, "%*pb\n", nmaskbits, maskp);
	return n;
}

static ssize_t
cpumap_print_to_pagebuf(bool list, char *buf, const struct cpumask *mask)
{
	return bitmap_print_to_pagebuf(list, buf, cpumask_bits(mask),
				      nr_cpu_ids);
}
*/

static ssize_t pcie_pmu_cpumask_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct pcie_pmu *pmu_pcie = dev_get_drvdata(dev);

	printk("cdns cpumask show\n");
        //return cpumap_print_to_pagebuf(true, buf, pmu_pcie->cpu);
        return 0;
}


static struct device_attribute pcie_pmu_cpumask_attr =
        __ATTR(cpumask, S_IRUGO, pcie_pmu_cpumask_show, NULL);


static struct attribute *pcie_pmu_cpumask_attrs[] = {
        &pcie_pmu_cpumask_attr.attr,
        NULL,
};


static struct attribute_group pcie_pmu_cpumask_attr_group = {
        .attrs = pcie_pmu_cpumask_attrs,
};


/////////////////////////////////////////////////////

PMU_FORMAT_ATTR(event, "config:0-63");
PMU_FORMAT_ATTR(axi_id, "config1:0-63");


static struct attribute *pcie_pmu_format_attrs[] = {
        &format_attr_event.attr,
        &format_attr_axi_id.attr,
        NULL,
};


///////////////////////////////////////////////////////
static struct attribute_group pcie_pmu_events_attr_group = {
        .name = "events",
        .attrs = pcie_pmu_events_attrs,
};

static struct attribute_group pcie_pmu_format_attr_group = {
        .name = "format",
        .attrs = pcie_pmu_format_attrs,
};
/////////////////////////////////////////////////////////////////

static const struct attribute_group *attr_groups[] = {
        &pcie_pmu_events_attr_group,
        &pcie_pmu_format_attr_group,
        &pcie_pmu_cpumask_attr_group,
        NULL,
};



struct pcie_pmu {
        struct pmu pmu;
        void __iomem *pcie_base;
        cpumask_t cpu;
        struct hrtimer hrtimer;
        unsigned int active_events;
        struct device *dev;
        struct perf_event *pcie_events[PCIE_NUM_COUNTERS];
        struct hlist_node node;
        struct fsl_pcie_devtype_data *devtype_data;
};
#define to_pcie_pmu(p) container_of(p, struct pcie_pmu, pmu)

static int my_readl(void __iomem *reg){
	printk("reg %d\n", reg);
	return 0;

}

#define TOTAL_CYCLES		0x0
#define BUSY_CYCLES		0x1
#define READ_ACCESSES		0x2
#define WRITE_ACCESSES		0x3
#define READ_BYTES		0x4
#define WRITE_BYTES		0x5

#define MMDC_MADPCR0	0x410
#define MMDC_MADPCR1	0x414
#define MMDC_MADPSR0	0x418
#define MMDC_MADPSR1	0x41C
#define MMDC_MADPSR2	0x420
#define MMDC_MADPSR3	0x424
#define MMDC_MADPSR4	0x428
#define MMDC_MADPSR5	0x42C


static bool pcie_pmu_group_is_valid(struct perf_event *event)
{

/*
	struct pmu *pmu = event->pmu;
	struct perf_event *leader = event->group_leader;
	struct perf_event *sibling;
	unsigned long counter_mask = 0;

	set_bit(leader->attr.config, &counter_mask);

	if (event != leader) {
		if (!pcie_pmu_group_event_is_valid(event, pmu, &counter_mask))
			return false;
	}

	list_for_each_entry(sibling, &leader->sibling_list, group_entry) {
		if (!pcie_pmu_group_event_is_valid(sibling, pmu, &counter_mask))
			return false;
	}
*/

	return true;
}

/*
 * Polling period is set to one second, overflow of total-cycles (the fastest
 * increasing counter) takes ten seconds so one second is safe
 */
static unsigned int pcie_pmu_poll_period_us = 1000000;

module_param_named(pmu_pmu_poll_period_us, pcie_pmu_poll_period_us, uint,
		S_IRUGO | S_IWUSR);

static ktime_t pcie_pmu_timer_period(void)
{
	return ns_to_ktime((u64)pcie_pmu_poll_period_us * 1000);
}

static void pcie_pmu_event_start(struct perf_event *event, int flags)
{

	struct pcie_pmu *pmu_pcie = to_pcie_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	void __iomem *pcie_base, *reg;
	u32 val;

	pcie_base = pmu_pcie->pcie_base;
	printk("cdns pcie_pmu_event_start %d\n", flags);

	/*
	 * hrtimer is required because pcie does not provide an interrupt so
	 * polling is necessary
	 */
	hrtimer_start(&pmu_pcie->hrtimer, pcie_pmu_timer_period(),
			HRTIMER_MODE_REL_PINNED);


}

static u32 pcie_pmu_read_counter(struct pcie_pmu *pmu_pcie, int cfg)
{
	void __iomem *pcie_base, *reg;

	pcie_base = pmu_pcie->pcie_base;

	switch (cfg) {
	case TOTAL_CYCLES:
		reg = pcie_base + MMDC_MADPSR0;
		break;
	case BUSY_CYCLES:
		reg = pcie_base + MMDC_MADPSR1;
		break;
	case READ_ACCESSES:
		reg = pcie_base + MMDC_MADPSR2;
		break;
	case WRITE_ACCESSES:
		reg = pcie_base + MMDC_MADPSR3;
		break;
	case READ_BYTES:
		reg = pcie_base + MMDC_MADPSR4;
		break;
	case WRITE_BYTES:
		reg = pcie_base + MMDC_MADPSR5;
		break;
	default:
		return WARN_ONCE(1,
			"invalid configuration %d for pcie counter", cfg);
	}
	return my_readl(reg);
}

static void pcie_pmu_event_display(struct perf_event *event){

	printk("perf_event display\n");
	printk("sample_period	%d\n", event->attr.sample_period);
	printk("attach_state	%d\n", event->attach_state);
	printk("cpu		%d\n", event->cpu);
	printk("cfg		%d\n", event->attr.config);
	event->cpu = 0;
	event->attach_state = 1;

}

static int pcie_pmu_event_init(struct perf_event *event)
{
	printk("cdns pcie_pmu_event_init\n");
	struct pcie_pmu *pmu_pcie = to_pcie_pmu(event->pmu);
	int cfg = event->attr.config;

	printk("%d - %d\n", event->attr.type,  event->pmu->type);

	pcie_pmu_event_display(event);

	if (event->attr.type != event->pmu->type)
		return -ENOENT;
	printk("1\n");
	if (is_sampling_event(event) || event->attach_state & PERF_ATTACH_TASK)
		return -EOPNOTSUPP;

	printk("2\n");
	if (event->cpu < 0) {
		printk("Can't provide per-task data!\n");
		return -EOPNOTSUPP;
	}

	printk("3\n");
/*
	if (event->attr.exclude_user		||
			event->attr.exclude_kernel	||
			event->attr.exclude_hv		||
			event->attr.exclude_idle	||
			event->attr.exclude_host	||
			event->attr.exclude_guest	||
			event->attr.sample_period)
		return -EINVAL;
*/
	printk("4\n");
	if (cfg < 0 || cfg >= PCIE_NUM_COUNTERS)
		return -EINVAL;

	printk("5\n");
	if (!pcie_pmu_group_is_valid(event))
		return -EINVAL;

	printk("6\n");
	event->cpu = cpumask_first(&pmu_pcie->cpu);
	printk("cdns sucess pcie_pmu_event_init %d\n",  event->cpu);
	return 0;

}

static int pcie_pmu_event_add(struct perf_event *event, int flags)
{
	printk("cdns pcie_pmu_event_add flag %d\n", flags);

	struct pcie_pmu *pmu_pcie = to_pcie_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	int cfg = event->attr.config;

	if (flags & PERF_EF_START)
		pcie_pmu_event_start(event, flags);

	if (pmu_pcie->pcie_events[cfg] != NULL)
		return -EAGAIN;

	pmu_pcie->pcie_events[cfg] = event;
	pmu_pcie->active_events++;

	local64_set(&hwc->prev_count, pcie_pmu_read_counter(pmu_pcie, cfg));	

	return 0;
}

static void pcie_pmu_event_update(struct perf_event *event)
{
	printk ("cdns pcie_pmu_event_update \n");
	struct pcie_pmu *pmu_pcie = to_pcie_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 delta, prev_raw_count, new_raw_count;

	do {
		prev_raw_count = local64_read(&hwc->prev_count);
		new_raw_count = pcie_pmu_read_counter(pmu_pcie,
						      event->attr.config);
	} while (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
		new_raw_count) != prev_raw_count);

	delta = (new_raw_count - prev_raw_count) & 0xFFFFFFFF;

	local64_add(delta, &event->count);
}


static void pcie_pmu_event_stop(struct perf_event *event, int flags)
{
	printk("cdns pcie_pmu_event_stop %d\n", flags);
	pcie_pmu_event_update(event);
}


static void pcie_pmu_event_del(struct perf_event *event, int flags)
{
	printk("cdns pcie_pmu_event_del flag %d\n", flags);
	struct pcie_pmu *pmu_pcie = to_pcie_pmu(event->pmu);
	int cfg = event->attr.config;

	pmu_pcie->pcie_events[cfg] = NULL;
	pmu_pcie->active_events--;

	if (pmu_pcie->active_events == 0)
		hrtimer_cancel(&pmu_pcie->hrtimer);

	pcie_pmu_event_stop(event, PERF_EF_UPDATE);
}


static void pcie_pmu_overflow_handler(struct pcie_pmu *pmu_pcie)
{
	int i;

	for (i = 0; i < PCIE_NUM_COUNTERS; i++) {
		struct perf_event *event = pmu_pcie->pcie_events[i];

		if (event)
			pcie_pmu_event_update(event);
	}
}

static enum hrtimer_restart pcie_pmu_timer_handler(struct hrtimer *hrtimer)
{
	struct pcie_pmu *pmu_pcie = container_of(hrtimer, struct pcie_pmu,
			hrtimer);

	pcie_pmu_overflow_handler(pmu_pcie);
	hrtimer_forward_now(hrtimer, pcie_pmu_timer_period());

	return HRTIMER_RESTART;
}


static int pcie_pmu_init(struct pcie_pmu *pmu_pcie,
                void __iomem *pcie_base, struct device *dev)
{
        int pcie_num = 0;
	
#ifdef FUTURE_DEV
	pcie_base = of_iomap(dev.of_node, 0);

	res = verify_cdns_controller(pcie_base + 0) ;
	if (res == ERROR){
		printk("No cadence contorller found\n");
		return -1;
	}

	/* not sure why we need to make cpu multi dynamic hot plug */
#endif

#ifdef HRTIMER
	hrtimer_init(&pmu_pcie->hrtimer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	pmu_pcie->hrtimer.function = pcie_pmu_timer_handler;
#endif

        *pmu_pcie = (struct pcie_pmu) {
                .pmu = (struct pmu) {
                        //.task_ctx_nr    = perf_invalid_context,
                        .task_ctx_nr    = perf_hw_context,
                        .attr_groups    = attr_groups,
                        .event_init     = pcie_pmu_event_init,
                        .add            = pcie_pmu_event_add,
                        .del            = pcie_pmu_event_del,
                        .start          = pcie_pmu_event_start,
                        .stop           = pcie_pmu_event_stop,
                        .read           = pcie_pmu_event_update,
                },
                .pcie_base = pcie_base,
                .dev = dev,
                .active_events = 0,
        };

        pcie_num = ida_simple_get(&pcie_ida, 0, 0, GFP_KERNEL);

        return pcie_num;
}

static int pcie_pmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct pcie_pmu *pmu_pcie = hlist_entry_safe(node, struct pcie_pmu, node);
	int target;

	if (!cpumask_test_and_clear_cpu(cpu, &pmu_pcie->cpu))
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	perf_pmu_migrate_context(&pmu_pcie->pmu, cpu, target);
	cpumask_set_cpu(target, &pmu_pcie->cpu);

	return 0;
}

static int imx_pcie_remove(struct platform_device *pdev)
{
	struct pcie_pmu *pmu_pcie = platform_get_drvdata(pdev);

	cpuhp_state_remove_instance_nocalls(cpuhp_pcie_state, &pmu_pcie->node);
	perf_pmu_unregister(&pmu_pcie->pmu);
	kfree(pmu_pcie);
	return 0;
}

static int imx_pcie_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	void __iomem *pcie_base, *reg;
	u32 val;
	int timeout = 0x400;
#if 0
	mmdc_base = of_iomap(np, 0);
	WARN_ON(!mmdc_base);

	reg = mmdc_base + MMDC_MDMISC;
	/* Get ddr type */
	val = readl_relaxed(reg);
	ddr_type = (val & BM_MMDC_MDMISC_DDR_TYPE) >>
		 BP_MMDC_MDMISC_DDR_TYPE;

	reg = mmdc_base + MMDC_MAPSR;

	/* Enable automatic power saving */
	val = readl_relaxed(reg);
	val &= ~(1 << BP_MMDC_MAPSR_PSD);
	writel_relaxed(val, reg);

	/* Ensure it's successfully enabled */
	while (!(readl_relaxed(reg) & 1 << BP_MMDC_MAPSR_PSS) && --timeout)
		cpu_relax();

	if (unlikely(!timeout)) {
		pr_warn("%s: failed to enable automatic power saving\n",
			__func__);
		return -EBUSY;
	}
	return imx_mmdc_perf_init(pdev, mmdc_base);
#else
	struct pcie_pmu *pmu_pcie; 
	struct device *dev = &pdev->dev;
	int ret;

	char *name = "cdns_pcie";

	pmu_pcie = kzalloc(sizeof(*pmu_pcie), GFP_KERNEL);
        if (!pmu_pcie) {
                printk("failed to allocate PMU device!\n");
                return -ENOMEM;
        }
	/* The first instance registers the hotplug state */
	if (!cpuhp_pcie_state) {
		ret = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
					      "perf/arm/pcie-cdns:online", NULL,
					      pcie_pmu_offline_cpu);
		if (ret < 0) {
			pr_err("cpuhp_setup_state_multi failed\n");
			goto pmu_free;
		}
		cpuhp_pcie_state = ret;
	}
	
	pcie_pmu_init(pmu_pcie, pcie_base, dev);


	hrtimer_init(&pmu_pcie->hrtimer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	pmu_pcie->hrtimer.function = pcie_pmu_timer_handler;

	cpumask_set_cpu(raw_smp_processor_id(), &pmu_pcie->cpu);

	/* Register the pmu instance for cpu hotplug */
	cpuhp_state_add_instance_nocalls(cpuhp_pcie_state, &pmu_pcie->node);


	printk(KERN_ALERT "Hello, world\n");
	ret = perf_pmu_register(&(pmu_pcie->pmu), name, -1);
	if (ret){
		printk("Error in pmu register %d\n", ret);
		return -1;
	}
pmu_free:
	kfree(pmu_pcie);
	return ret;
#endif

}

static struct platform_driver imx_pcie_driver = {
	.driver		= {
		.name	= "imx-pcie",
		.of_match_table = imx_pcie_dt_ids,
	},
	.probe		= imx_pcie_probe,
	.remove		= imx_pcie_remove,
};

struct pcie_pmu  *pmu_pcie;

static int perfpcie_init(void) {
	int i, ret;

/*
	ret = cpuhp_setup_state_multi(CPUHP_AP_PERF_ARM_CCN_ONLINE,
				      "perf/arm/ccn:online", NULL,
				      arm_ccn_pmu_offline_cpu);
	if (ret)
		return ret;
	for (i = 0; i < ARRAY_SIZE(arm_ccn_pmu_events); i++)
		arm_ccn_pmu_events_attrs[i] = &arm_ccn_pmu_events[i].attr.attr;
*/

	ret = platform_driver_register(&imx_pcie_driver);
/*
	if (ret)
		cpuhp_remove_multi_state(CPUHP_AP_PERF_ARM_CCN_ONLINE);
*/
	printk("PMU registerd %d\n", ret);
	return ret;
	return 0;
pmu_free:
	kfree(pmu_pcie);
	return ret;
}
static void perfpcie_exit(void)
{
	printk(KERN_ALERT "Goodbye, cruel world %d\n", cpuhp_pcie_state);
	//cpuhp_state_remove_instance_nocalls(cpuhp_pcie_state, &pmu_pcie->node);
	cpuhp_remove_multi_state(CPUHP_AP_ONLINE_DYN);
	hrtimer_cancel(&pmu_pcie->hrtimer);
	perf_pmu_unregister(&(pmu_pcie->pmu));
	kfree(pmu_pcie);
}

module_init(perfpcie_init);
module_exit(perfpcie_exit);

