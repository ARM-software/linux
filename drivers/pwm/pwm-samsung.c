/* drivers/pwm/pwm-samsung.c
 *
 * Copyright (c) 2007 Ben Dooks
 * Copyright (c) 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>, <ben-linux@fluff.org>
 *
 * S3C series PWM device core
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
*/

#define pr_fmt(fmt) "pwm-samsung: " fmt

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/of.h>
#include <linux/spinlock.h>

#define NPWM				4

#define REG_TCFG0			0x00
#define REG_TCFG1			0x04
#define REG_TCON			0x08
#define REG_TINT_CSTAT			0x44

#define REG_TCNTB(chan)			(0x0c + 12 * (chan))
#define REG_TCMPB(chan)			(0x10 + 12 * (chan))

#define to_s3c_chip(chip)		container_of(chip, struct s3c_chip, chip)

#define pwm_dbg(_pwm, msg...)		dev_dbg(&(_pwm)->pdev->dev, msg)
#define pwm_info(_pwm, msg...)		dev_info(&(_pwm)->pdev->dev, msg)

#define pwm_tcon_start(pwm)		(1 << (pwm->tcon_base + 0))
#define pwm_tcon_invert(pwm)		(1 << (pwm->tcon_base + 2))
#define pwm_tcon_autoreload(pwm)	(1 << (pwm->tcon_base + 3))
#define pwm_tcon_manulupdate(pwm)	(1 << (pwm->tcon_base + 1))

#define pwm_is_s3c24xx(s3c)		(s3c->pwm_type == S3C24XX_PWM)

#define NS_IN_HZ			(1000000000UL)

enum duty_cycle {
	DUTY_CYCLE_ZERO,
	DUTY_CYCLE_PULSE,
	DUTY_CYCLE_FULL,
};

enum s3c_pwm_type {
	S3C24XX_PWM = 0,
	S3C64XX_PWM,
};

struct s3c_pwm_device {
	struct clk		*clk_div;
	struct clk		*clk_tin;

	unsigned int		period_ns;
	unsigned int		duty_ns;

	unsigned char		tcon_base;
	unsigned char		running;
	unsigned char		requested;
	unsigned char		pwm_id;

	enum duty_cycle		duty_cycle;
	struct pwm_device	*pwm;
};

struct s3c_chip {
	struct platform_device	*pdev;
	struct clk		*clk;
	struct clk		*ipclk;
	void __iomem		*reg_base;
	struct pwm_chip		chip;
	struct s3c_pwm_device	*s3c_pwm[NPWM];
	unsigned int		pwm_type;
	unsigned int		reg_tcfg0;
};

static DEFINE_SPINLOCK(pwm_spinlock);

static inline int pwm_is_tdiv(struct s3c_pwm_device *s3c_pwm)
{
	return clk_get_parent(s3c_pwm->clk_tin) == s3c_pwm->clk_div;
}

static int s3c_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct s3c_chip *s3c = to_s3c_chip(chip);
	struct s3c_pwm_device *s3c_pwm = pwm_get_chip_data(pwm);
	void __iomem *reg_base = s3c->reg_base;
	unsigned long flags;
	unsigned long tcon;

	spin_lock_irqsave(&pwm_spinlock, flags);

	if (pwm_is_s3c24xx(s3c)) {
		tcon = __raw_readl(reg_base + REG_TCON);
		tcon |= pwm_tcon_start(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);
	} else {
		tcon = __raw_readl(reg_base + REG_TCON);
		if (!(tcon & pwm_tcon_start(s3c_pwm))) {
			tcon |= pwm_tcon_manulupdate(s3c_pwm);
			__raw_writel(tcon, reg_base + REG_TCON);

			tcon &= ~pwm_tcon_manulupdate(s3c_pwm);
			if (s3c_pwm->duty_cycle == DUTY_CYCLE_ZERO)
				tcon &= ~pwm_tcon_autoreload(s3c_pwm);
			else
				tcon |= pwm_tcon_autoreload(s3c_pwm);
			tcon |= pwm_tcon_start(s3c_pwm);
			__raw_writel(tcon, reg_base + REG_TCON);
		} else if (!(tcon & pwm_tcon_autoreload(s3c_pwm)) &&
			   s3c_pwm->duty_cycle != DUTY_CYCLE_ZERO) {
			tcon |= pwm_tcon_manulupdate(s3c_pwm);
			__raw_writel(tcon, reg_base + REG_TCON);

			tcon &= ~pwm_tcon_manulupdate(s3c_pwm);
			tcon |= pwm_tcon_autoreload(s3c_pwm);
			__raw_writel(tcon, reg_base + REG_TCON);
		}
	}

	s3c_pwm->running = 1;

	spin_unlock_irqrestore(&pwm_spinlock, flags);
	return 0;
}

static void s3c_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct s3c_chip *s3c = to_s3c_chip(chip);
	struct s3c_pwm_device *s3c_pwm = pwm_get_chip_data(pwm);
	void __iomem *reg_base = s3c->reg_base;
	unsigned long flags;
	unsigned long tcon;

	spin_lock_irqsave(&pwm_spinlock, flags);

	if (pwm_is_s3c24xx(s3c)) {
		tcon = __raw_readl(reg_base + REG_TCON);
		tcon &= ~pwm_tcon_start(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);
	} else {
		tcon = __raw_readl(reg_base + REG_TCON);
		tcon &= ~pwm_tcon_autoreload(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);
	}

	s3c_pwm->running = 0;

	spin_unlock_irqrestore(&pwm_spinlock, flags);
}

static unsigned long pwm_calc_tin(struct pwm_device *pwm, unsigned long freq)
{
	struct s3c_pwm_device *s3c_pwm = pwm_get_chip_data(pwm);
	unsigned long tin_parent_rate;
	unsigned int div;

	tin_parent_rate = clk_get_rate(clk_get_parent(s3c_pwm->clk_div));
	pr_info("tin parent at %lu\n", tin_parent_rate);

	for (div = 2; div <= 16; div *= 2) {
		if ((tin_parent_rate / (div << 16)) < freq)
			return tin_parent_rate / div;
	}

	return tin_parent_rate / 16;
}

static int s3c_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
		int duty_ns, int period_ns)
{
	struct s3c_chip *s3c = to_s3c_chip(chip);
	struct s3c_pwm_device *s3c_pwm = pwm_get_chip_data(pwm);
	void __iomem *reg_base = s3c->reg_base;
	unsigned long tin_rate;
	unsigned long tin_ns;
	unsigned long period;
	unsigned long flags;
	unsigned long tcon;
	unsigned long tcnt;
	long tcmp;
	enum duty_cycle duty_cycle;
	unsigned int id = pwm->pwm;

	/* We currently avoid using 64bit arithmetic by using the
	 * fact that anything faster than 1Hz is easily representable
	 * by 32bits. */

	if (period_ns > NS_IN_HZ || duty_ns > NS_IN_HZ)
		return -ERANGE;

	if (duty_ns > period_ns)
		return -EINVAL;

	if (period_ns == s3c_pwm->period_ns &&
	    duty_ns == s3c_pwm->duty_ns)
		return 0;

	period = NS_IN_HZ / period_ns;

	pwm_info(s3c, "duty_ns=%d, period_ns=%d (%lu)\n",
			duty_ns, period_ns, period);

	/* Check to see if we are changing the clock rate of the PWM */

	if (s3c_pwm->period_ns != period_ns && pwm_is_tdiv(s3c_pwm)) {
		tin_rate = pwm_calc_tin(pwm, period);
		clk_set_rate(s3c_pwm->clk_div, tin_rate);
		tin_rate = clk_get_rate(s3c_pwm->clk_div);
		s3c_pwm->period_ns = period_ns;
		pwm_dbg(s3c, "tin_rate=%lu\n", tin_rate);
	} else {
		tin_rate = clk_get_rate(s3c_pwm->clk_tin);
	}

	/* Note, counters count down */
	tin_ns = NS_IN_HZ / tin_rate;

	tcnt = DIV_ROUND_CLOSEST(period_ns, tin_ns);
	tcmp = DIV_ROUND_CLOSEST(duty_ns, tin_ns);

	if (tcnt <= 1) {
		/* Too small to generate a pulse */
		return -ERANGE;
	}

	pwm_dbg(s3c, "duty_ns=%d, period_ns=%d (%lu)\n",
		duty_ns, period_ns, period);

	if (tcmp == 0)
		duty_cycle = DUTY_CYCLE_ZERO;
	else if (tcmp == tcnt)
		duty_cycle = DUTY_CYCLE_FULL;
	else
		duty_cycle = DUTY_CYCLE_PULSE;

	tcmp = tcnt - tcmp;
	/* the pwm hw only checks the compare register after a decrement,
	   so the pin never toggles if tcmp = tcnt */
	if (tcmp == tcnt)
		tcmp--;

	/*
	 * PWM counts 1 hidden tick at the end of each period on S3C64XX and
	 * EXYNOS series, so tcmp and tcnt should be subtracted 1.
	 */
	if (!pwm_is_s3c24xx(s3c)) {
		tcnt--;
		/*
		 * tcmp can be -1. It appears 100% duty cycle and PWM never
		 * toggles when TCMPB is set to 0xFFFFFFFF (-1).
		 */
		tcmp--;
	}

	pwm_dbg(s3c, "tin_ns=%lu, tcmp=%ld/%lu\n", tin_ns, tcmp, tcnt);

	/* Update the PWM register block. */

	spin_lock_irqsave(&pwm_spinlock, flags);

	__raw_writel(tcmp, reg_base + REG_TCMPB(id));
	__raw_writel(tcnt, reg_base + REG_TCNTB(id));

	if (pwm_is_s3c24xx(s3c)) {
		tcon = __raw_readl(reg_base + REG_TCON);
		tcon |= pwm_tcon_manulupdate(s3c_pwm);
		tcon |= pwm_tcon_autoreload(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);

		tcon &= ~pwm_tcon_manulupdate(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);
	} else {
		tcon = __raw_readl(reg_base + REG_TCON);
		if (s3c_pwm->running == 1 &&
		    tcon & pwm_tcon_start(s3c_pwm) &&
		    s3c_pwm->duty_cycle != duty_cycle) {
			if (duty_cycle == DUTY_CYCLE_ZERO) {
				tcon |= pwm_tcon_manulupdate(s3c_pwm);
				__raw_writel(tcon, reg_base + REG_TCON);

				tcon &= ~pwm_tcon_manulupdate(s3c_pwm);
				tcon &= ~pwm_tcon_autoreload(s3c_pwm);
			} else {
				tcon |= pwm_tcon_autoreload(s3c_pwm);
			}
			__raw_writel(tcon, reg_base + REG_TCON);
		}
	}
	s3c_pwm->duty_ns = duty_ns;
	s3c_pwm->period_ns = period_ns;
	s3c_pwm->duty_cycle = duty_cycle;

	spin_unlock_irqrestore(&pwm_spinlock, flags);

	return 0;
}

static void s3c_pwm_init(struct s3c_chip *s3c, struct s3c_pwm_device *s3c_pwm)
{
	void __iomem *reg_base = s3c->reg_base;
	unsigned long tcon;
	unsigned int id = s3c_pwm->pwm_id;

	if (pwm_is_s3c24xx(s3c)) {
		tcon = __raw_readl(reg_base + REG_TCON);
		tcon |= pwm_tcon_invert(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);
	} else {
		__raw_writel(0, reg_base + REG_TCMPB(id));
		__raw_writel(0, reg_base + REG_TCNTB(id));

		tcon = __raw_readl(reg_base + REG_TCON);
		tcon |= pwm_tcon_invert(s3c_pwm);
		tcon |= pwm_tcon_manulupdate(s3c_pwm);
		tcon &= ~pwm_tcon_autoreload(s3c_pwm);
		tcon &= ~pwm_tcon_start(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);

		tcon &= ~pwm_tcon_manulupdate(s3c_pwm);
		__raw_writel(tcon, reg_base + REG_TCON);
	}
}

static int s3c_pwm_request(struct pwm_chip *chip,
				 struct pwm_device *pwm)
{
	struct s3c_chip *s3c = to_s3c_chip(chip);
	struct s3c_pwm_device *s3c_pwm;
	struct device *dev = chip->dev;
	void __iomem *reg_base = s3c->reg_base;
	unsigned char clk_tin_name[16];
	unsigned char clk_tdiv_name[16];
	static struct clk *clk_timers;
	unsigned int id = pwm->pwm;
	unsigned long flags;
	int ret;

	s3c_pwm = devm_kzalloc(chip->dev, sizeof(*s3c_pwm), GFP_KERNEL);
	if (!s3c_pwm)
		return -ENOMEM;

	/* calculate base of control bits in TCON */
	s3c_pwm->tcon_base = id == 0 ? 0 : (id * 4) + 4;
	s3c_pwm->pwm_id = id;

	s3c_pwm->pwm = pwm;
	pwm_set_chip_data(pwm, s3c_pwm);

	snprintf(clk_tin_name, sizeof(clk_tin_name), "pwm-tin%d", id);
	clk_timers = devm_clk_get(dev, clk_tin_name);
	if (IS_ERR(clk_timers)) {
		dev_err(dev, "failed to get pwm tin clk\n");
		ret = PTR_ERR(clk_timers);
		goto err;
	}
	s3c_pwm->clk_tin = clk_timers;

	snprintf(clk_tdiv_name, sizeof(clk_tdiv_name), "pwm-tdiv%d", id);
	clk_timers = devm_clk_get(dev, clk_tdiv_name);
	if (IS_ERR(clk_timers)) {
		dev_err(dev, "failed to get pwm tdiv clk\n");
		ret = PTR_ERR(clk_timers);
		goto err;
	}
	s3c_pwm->clk_div = clk_timers;

	spin_lock_irqsave(&pwm_spinlock, flags);
	s3c_pwm_init(s3c, s3c_pwm);
	spin_unlock_irqrestore(&pwm_spinlock, flags);

	s3c->s3c_pwm[id] = s3c_pwm;

	pwm_dbg(s3c, "config bits %02x\n",
		(__raw_readl(reg_base + REG_TCON) >> s3c_pwm->tcon_base) & 0x0f);
	return 0;

err:
	devm_kfree(chip->dev, s3c_pwm);
	return ret;
}

static void s3c_pwm_free(struct pwm_chip *chip,
				struct pwm_device *pwm)
{
	struct s3c_chip *s3c = to_s3c_chip(chip);
	struct s3c_pwm_device *s3c_pwm = pwm_get_chip_data(pwm);
	unsigned int id = pwm->pwm;

	s3c->s3c_pwm[id] = NULL;
	devm_kfree(chip->dev, s3c_pwm);
}

static struct pwm_ops s3c_pwm_ops = {
	.request = s3c_pwm_request,
	.free = s3c_pwm_free,
	.enable = s3c_pwm_enable,
	.disable = s3c_pwm_disable,
	.config = s3c_pwm_config,
	.owner = THIS_MODULE,
};

static int s3c_pwm_clk_init(struct platform_device *pdev,
				struct s3c_chip *s3c)
{
	struct device *dev = &pdev->dev;
	static struct clk *clk_scaler[2];

	s3c->ipclk = devm_clk_get(dev, "ipclk");
	if (IS_ERR(s3c->ipclk)) {
		pr_err("no parent ip clock\n");
		return -EINVAL;
	}

	clk_prepare_enable(s3c->ipclk);

	s3c->clk = devm_clk_get(dev, "gate_timers");
	if (IS_ERR(s3c->clk)) {
		pr_err("no parent clock\n");
		return -EINVAL;
	}

	clk_prepare_enable(s3c->clk);

	clk_scaler[0] = devm_clk_get(dev, "pwm-scaler0");
	clk_scaler[1] = devm_clk_get(dev, "pwm-scaler1");

	if (IS_ERR(clk_scaler[0]) || IS_ERR(clk_scaler[1])) {
		pr_err("failed to get scaler clocks\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id s3c_pwm_match[] = {
	{
		.compatible = "samsung,s3c2410-pwm",
		.data = (void *)S3C24XX_PWM,
	},
	{
		.compatible = "samsung,s3c6400-pwm",
		.data = (void *)S3C64XX_PWM,
	},
	{ }
};
#endif

static inline int s3c_pwm_get_driver_data(struct device *dev)
{
#ifdef CONFIG_OF
	if (dev->of_node) {
		const struct of_device_id *match;
		match = of_match_node(s3c_pwm_match, dev->of_node);
		return (int)match->data;
	}
#endif
	return 0;
}

static int s3c_pwm_probe(struct platform_device *pdev)
{
	static struct resource	*pwm_mem;
	struct device *dev = &pdev->dev;
	struct s3c_chip *s3c;
	int ret;

	pwm_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (pwm_mem == NULL) {
		dev_err(dev, "no memory resource specified\n");
		return -ENOENT;
	}

	s3c = devm_kzalloc(&pdev->dev, sizeof(*s3c), GFP_KERNEL);
	if (s3c == NULL) {
		dev_err(dev, "failed to allocate pwm_device\n");
		ret = -ENOMEM;
		return ret;
	}

	s3c->reg_base = devm_ioremap_resource(dev, pwm_mem);
	if (!s3c->reg_base) {
		dev_err(dev, "failed to map pwm registers\n");
		ret = PTR_ERR(s3c->reg_base);
		return ret;
	}

	ret = s3c_pwm_clk_init(pdev, s3c);
	if (ret < 0) {
		dev_err(dev, "failed to pwm clock init\n");
		return ret;
	}

	s3c->pdev = pdev;
	s3c->chip.dev = &pdev->dev;
	s3c->chip.ops = &s3c_pwm_ops;
	s3c->chip.of_xlate = of_pwm_xlate_with_flags;
	s3c->chip.of_pwm_n_cells = 3;
	s3c->chip.base = -1;
	s3c->chip.npwm = NPWM;
	s3c->pwm_type = s3c_pwm_get_driver_data(dev);

	ret = pwmchip_add(&s3c->chip);
	if (ret < 0) {
		dev_err(dev, "failed to register pwm\n");
		return ret;
	}

	platform_set_drvdata(pdev, s3c);
	return 0;
}

static int s3c_pwm_remove(struct platform_device *pdev)
{
	struct s3c_chip *s3c = platform_get_drvdata(pdev);
	int err;

	err = pwmchip_remove(&s3c->chip);
	if (err < 0)
		return err;

	clk_disable_unprepare(s3c->clk);
	clk_disable_unprepare(s3c->ipclk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int s3c_pwm_suspend(struct device *dev)
{
	struct s3c_chip *s3c = dev_get_drvdata(dev);
	struct s3c_pwm_device *s3c_pwm;
	void __iomem *reg_base = s3c->reg_base;
	unsigned long tcon;
	unsigned char i;

	if (!pwm_is_s3c24xx(s3c)) {
		for (i = 0; i < NPWM; i++) {
			if (s3c->s3c_pwm[i] == NULL)
				continue;
			s3c_pwm = s3c->s3c_pwm[i];
			if (s3c_pwm->running == 0) {
				tcon = __raw_readl(reg_base + REG_TCON);
				if (s3c_pwm->duty_cycle == DUTY_CYCLE_ZERO) {
					tcon |= pwm_tcon_manulupdate(s3c_pwm);
				} else if (s3c_pwm->duty_cycle == DUTY_CYCLE_FULL) {
					tcon &= pwm_tcon_invert(s3c_pwm);
					tcon |= pwm_tcon_manulupdate(s3c_pwm);
				}
				tcon &= ~pwm_tcon_start(s3c_pwm);
				__raw_writel(tcon, reg_base + REG_TCON);
			}
			/* No one preserve these values during suspend so reset them
			 * Otherwise driver leaves PWM unconfigured if same values
			 * passed to pwm_config
			 */
			s3c_pwm->duty_ns = -1;
			s3c_pwm->period_ns = -1;
		}
	}
	/* Save pwm registers*/
	s3c->reg_tcfg0 = __raw_readl(s3c->reg_base + REG_TCFG0);

	clk_disable(s3c->clk);
	clk_disable(s3c->ipclk);
	return 0;
}

static int s3c_pwm_resume(struct device *dev)
{
	struct s3c_chip *s3c = dev_get_drvdata(dev);
	struct s3c_pwm_device *s3c_pwm;
	unsigned char i;

	clk_enable(s3c->ipclk);
	clk_enable(s3c->clk);

	/* Restore pwm registers*/
	__raw_writel(s3c->reg_tcfg0, s3c->reg_base + REG_TCFG0);

	for (i = 0; i < NPWM; i++) {
		if (s3c->s3c_pwm[i] == NULL)
			continue;
		s3c_pwm = s3c->s3c_pwm[i];
		s3c_pwm_init(s3c, s3c_pwm);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(s3c_pwm_pm_ops, s3c_pwm_suspend,
			s3c_pwm_resume);

static struct platform_driver s3c_pwm_driver = {
	.driver		= {
		.name	= "s3c24xx-pwm",
		.owner	= THIS_MODULE,
		.pm	= &s3c_pwm_pm_ops,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(s3c_pwm_match),
#endif
	},
	.probe		= s3c_pwm_probe,
	.remove		= s3c_pwm_remove,
};

static int __init pwm_init(void)
{
	int ret;

	ret = platform_driver_register(&s3c_pwm_driver);
	if (ret)
		pr_err("failed to add pwm driver\n");

	return ret;
}

arch_initcall(pwm_init);
