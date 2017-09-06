#include <linux/cpumask.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/perf_event.h>
#include <linux/slab.h>


MODULE_LICENSE("Dual BSD/GPL");

#define PCIE_NUM_COUNTERS			7

#define CDNS_PCIE_LOCAL_MGMT_ADDR 		0x00100000

#define TX_TLP_COUNT_EVENT 			0x0
#define TX_TLP_PAYLOAD_COUNT_EVENT		0x1
#define RX_TLP_COUNT_EVENT 			0x2
#define RX_TLP_PAYLOAD_COUNT_EVENT 		0x3
#define LCRC_ERROR_COUNT_EVENT			0x4
#define ECC_CORRECTABLE_ERROR_COUNT_EVENT	0x5

#define TX_TLP_COUNT_REG			0x028
#define TX_TLP_PAYLOAD_COUNT_REG		0x02c
#define RX_TLP_COUNT_REG			0x030
#define RX_TLP_PAYLOAD_COUNT_REG		0x034
#define LCRC_ERROR_COUNT_REG			0x214
#define ECC_CORRECTABLE_ERROR_COUNT_REG		0x218



static DEFINE_IDA(pcie_ida);


static const struct of_device_id gen_pci_of_match[] = {
        { .compatible = "cdns,pci-cdns-perf" },
        { },
};

MODULE_DEVICE_TABLE(of, gen_pci_of_match);

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


PMU_EVENT_ATTR_STRING(TxTLPcount, pcie_pmu_TxTLPcount, "event=0x00")
PMU_EVENT_ATTR_STRING(TxTLPpayloadCount, pcie_pmu_TxTLPpayloadCount, "event=0x01")
PMU_EVENT_ATTR_STRING(RxTLPcount, pcie_pmu_RxTLPcount, "event=0x02")
PMU_EVENT_ATTR_STRING(RxTLPpayloadCount, pcie_pmu_RxTLPpayloadCount, "event=0x03")
PMU_EVENT_ATTR_STRING(LCRCErrorCount, pcie_pmu_LCRCErrorCount, "event=0x04")
PMU_EVENT_ATTR_STRING(ECCcorrectableErrorCount, pcie_pmu_ECCcorrectableErrorCount, "event=0x05")
PMU_EVENT_ATTR_STRING(ECCcorrectableErrorCountRegisterforAXIRAMs, pcie_pmu_ECCcorrectableErrorCountRegisterforAXIRAMs, "event=0x06")

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


static ssize_t pcie_pmu_cpumask_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct pcie_pmu *pmu_pcie = dev_get_drvdata(dev);

       	return cpumap_print_to_pagebuf(true, buf, &pmu_pcie->cpu);
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


PMU_FORMAT_ATTR(event, "config:0-63");
PMU_FORMAT_ATTR(axi_id, "config1:0-63");


static struct attribute *pcie_pmu_format_attrs[] = {
        &format_attr_event.attr,
        &format_attr_axi_id.attr,
        NULL,
};

static struct attribute_group pcie_pmu_events_attr_group = {
        .name = "events",
        .attrs = pcie_pmu_events_attrs,
};

static struct attribute_group pcie_pmu_format_attr_group = {
        .name = "format",
        .attrs = pcie_pmu_format_attrs,
};

static const struct attribute_group *attr_groups[] = {
        &pcie_pmu_events_attr_group,
        &pcie_pmu_format_attr_group,
        &pcie_pmu_cpumask_attr_group,
        NULL,
};



#define to_pcie_pmu(p) container_of(p, struct pcie_pmu, pmu)

static int inline my_readl(void __iomem *reg){
	return *(volatile uint32_t *)reg;

}

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

	//pcie_base = pmu_pcie->pcie_base;
	pcie_base = 0xfb000000;

	switch (cfg) {
	case TX_TLP_COUNT_EVENT:
		reg = pcie_base | TX_TLP_COUNT_REG | CDNS_PCIE_LOCAL_MGMT_ADDR;
		break;
	case TX_TLP_PAYLOAD_COUNT_EVENT:
		reg = pcie_base | TX_TLP_PAYLOAD_COUNT_REG | CDNS_PCIE_LOCAL_MGMT_ADDR;
		break;
	case RX_TLP_COUNT_EVENT:
		reg = pcie_base | RX_TLP_COUNT_REG | CDNS_PCIE_LOCAL_MGMT_ADDR;
		break;
	case RX_TLP_PAYLOAD_COUNT_EVENT:
		reg = pcie_base | RX_TLP_PAYLOAD_COUNT_REG | CDNS_PCIE_LOCAL_MGMT_ADDR;
		break;
	case LCRC_ERROR_COUNT_EVENT:
		reg = pcie_base | LCRC_ERROR_COUNT_REG | CDNS_PCIE_LOCAL_MGMT_ADDR;
		break;
	case ECC_CORRECTABLE_ERROR_COUNT_EVENT:
		reg = pcie_base | ECC_CORRECTABLE_ERROR_COUNT_REG | CDNS_PCIE_LOCAL_MGMT_ADDR;
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
#if 0
	struct pcie_pmu *pmu_pcie = to_pcie_pmu(event->pmu);
	int cfg = event->attr.config;

	//pcie_pmu_event_display(event);
/*
	if (event->attr.type != event->pmu->type)
		return -ENOENT;
	if (is_sampling_event(event) || event->attach_state & PERF_ATTACH_TASK)
		return -EOPNOTSUPP;

	if (event->cpu < 0) {
		printk("Can't provide per-task data!\n");
		return -EOPNOTSUPP;
	}

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
	if (cfg < 0 || cfg >= PCIE_NUM_COUNTERS)
		return -EINVAL;

	if (!pcie_pmu_group_is_valid(event))
		return -EINVAL;

	event->cpu = cpumask_first(&pmu_pcie->cpu);
#endif
	return 0;

}

static int pcie_pmu_event_add(struct perf_event *event, int flags)
{
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
	pcie_pmu_event_update(event);
}


static void pcie_pmu_event_del(struct perf_event *event, int flags)
{
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
        int pcie_num;
	
#ifdef FUTURE_DEV
	pcie_base = of_iomap(dev.of_node, 0);

	res = verify_cdns_controller(pcie_base + 0) ;
	if (res == ERROR){
		printk("No cadence contorller found\n");
		return -1;
	}

	/* not sure why we need to make cpu multi dynamic hot plug */
#endif

        *pmu_pcie = (struct pcie_pmu) {
                .pmu = (struct pmu) {
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

static int cdns_perf_pcie_init(struct platform_device *pdev, void __iomem *pcie_base)
{
	struct pcie_pmu *pmu_pcie;
	char *name;
	int pcie_num;
	int ret;
	const struct of_device_id *of_id =
		of_match_device(gen_pci_of_match, &pdev->dev);

	pmu_pcie = kzalloc(sizeof(*pmu_pcie), GFP_KERNEL);
	if (!pmu_pcie) {
		pr_err("failed to allocate PMU device!\n");
		return -ENOMEM;
	}

	pcie_num = pcie_pmu_init(pmu_pcie, pcie_base, &pdev->dev);
	name = "cdnsperf"; //TODO
	
	hrtimer_init(&pmu_pcie->hrtimer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	pmu_pcie->hrtimer.function = pcie_pmu_timer_handler;

	//cpumask_set_cpu(raw_smp_processor_id(), &pmu_pcie->cpu); //TODO
	
	ret = perf_pmu_register(&(pmu_pcie->pmu), name, -1);
	if (ret)
		goto pmu_register_err;

	platform_set_drvdata(pdev, pmu_pcie);
	return 0;

pmu_register_err:
	pr_warn("cdns Perf PMU failed (%d), disabled\n", ret);
	hrtimer_cancel(&pmu_pcie->hrtimer);
	kfree(pmu_pcie);
	return ret;
}

static int cdns_perf_pcie_remove(struct platform_device *pdev)
{
	struct pcie_pmu *pmu_pcie = platform_get_drvdata(pdev);
#ifdef CPUHP
	cpuhp_state_remove_instance_nocalls(cpuhp_pcie_state, &pmu_pcie->node);
#endif
	perf_pmu_unregister(&pmu_pcie->pmu);
	kfree(pmu_pcie);
	return 0;
}
static int cdns_perf_pcie_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	void __iomem *pcie_base, *reg;
	u32 val;
	int timeout = 0x400;
	pcie_base = of_iomap(np, 0);

	WARN_ON(!pcie_base);
		
	return cdns_perf_pcie_init(pdev, pcie_base);
}

static struct platform_driver imx_pcie_driver = {
	.driver		= {
		.name	= "pci-cdns-perf",
		.of_match_table = gen_pci_of_match,
		.owner = THIS_MODULE,
	},
	.probe		= cdns_perf_pcie_probe,
	.remove		= cdns_perf_pcie_remove,
};


static int perfpcie_init(void) {
	int ret;
	ret = platform_driver_register(&imx_pcie_driver);
	return ret;
}
static void perfpcie_exit(void)
{
	platform_driver_unregister(&imx_pcie_driver);
}

module_init(perfpcie_init);
module_exit(perfpcie_exit);
