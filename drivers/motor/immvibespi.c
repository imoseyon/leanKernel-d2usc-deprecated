/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/
#include <linux/delay.h>
#include <linux/gpio.h>
#include "tspdrv.h"

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1
#define PWM_DUTY_MAX    213 /* 13MHz / (579 + 1) = 22.4kHz */
#define PWM_DEVICE	1

static bool g_bampenabled;
static u8 pwr_on;

static int32_t vibe_set_pwm_freq(int nForce)
{
	/* Put the MND counter in reset mode for programming */
	HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_MNCNTR_EN_BMSK, 0);
	HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_PRE_DIV_SEL_BMSK,
	 3 << HWIO_GP_NS_REG_PRE_DIV_SEL_SHFT);
	HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_SRC_SEL_BMSK,
	 5 << HWIO_GP_NS_REG_SRC_SEL_SHFT);
	HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_MNCNTR_MODE_BMSK,
	 2 << HWIO_GP_NS_REG_MNCNTR_MODE_SHFT);
	HWIO_OUTM(GP_MD_REG, HWIO_GP_MD_REG_M_VAL_BMSK,
	 g_nlra_gp_clk_m << HWIO_GP_MD_REG_M_VAL_SHFT);
	if (nForce > 0)
		g_nforce_32 = ((nForce * g_nlra_gp_clk_pwm_mul) >> 8) + 1;
	else
		g_nforce_32 = ((nForce * g_nlra_gp_clk_pwm_mul) >> 8)
			+ g_nlra_gp_clk_d;
	/* printk(KERN_DEBUG "[tspdrv] g_nForce_32 : %d\n", g_nforce_32); */
	HWIO_OUTM(GP_MD_REG, HWIO_GP_MD_REG_D_VAL_BMSK,
	 (~((int16_t)g_nforce_32 << 1)) << HWIO_GP_MD_REG_D_VAL_SHFT);
	HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_GP_N_VAL_BMSK,
	 ~(g_nlra_gp_clk_n - g_nlra_gp_clk_m) << HWIO_GP_NS_REG_GP_N_VAL_SHFT);
	if (nForce > 0) {
			HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_GP_CLK_INV_BMSK,
			 1 << HWIO_GP_NS_REG_GP_CLK_INV_SHFT);
	} else {
			HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_GP_CLK_INV_BMSK,
			 0 << HWIO_GP_NS_REG_GP_CLK_INV_SHFT);
	}
	HWIO_OUTM(GP_NS_REG, HWIO_GP_NS_REG_MNCNTR_EN_BMSK,
	 1 << HWIO_GP_NS_REG_MNCNTR_EN_SHFT);
	/* printk(KERN_DEBUG
	 "[tspdrv]%x, %x, %x\n",
	 (~((int16_t)g_nforce_32 << 1)) << HWIO_GP_MD_REG_D_VAL_SHFT,
	 ~(g_nlra_gp_clk_n - g_nlra_gp_clk_m) << HWIO_GP_NS_REG_GP_N_VAL_SHFT,
	 1 << HWIO_GP_NS_REG_MNCNTR_EN_SHFT); */

	return VIBE_S_SUCCESS;
}

static int32_t vibe_pwm_onoff(u8 onoff)
{
	if (onoff) {
		HWIO_OUTM(GP_NS_REG,
				  HWIO_GP_NS_REG_GP_CLK_BRANCH_ENA_BMSK,
				  1<<HWIO_GP_NS_REG_GP_CLK_BRANCH_ENA_SHFT);
		HWIO_OUTM(GP_NS_REG,
				  HWIO_GP_NS_REG_GP_ROOT_ENA_BMSK,
				  1<<HWIO_GP_NS_REG_GP_ROOT_ENA_SHFT);
	} else {
		HWIO_OUTM(GP_NS_REG,
				  HWIO_GP_NS_REG_GP_CLK_BRANCH_ENA_BMSK,
				  0<<HWIO_GP_NS_REG_GP_CLK_BRANCH_ENA_SHFT);
		HWIO_OUTM(GP_NS_REG,
				  HWIO_GP_NS_REG_GP_ROOT_ENA_BMSK,
				  0<<HWIO_GP_NS_REG_GP_ROOT_ENA_SHFT);
	}
	return VIBE_S_SUCCESS;
}


/*
** Called to disable amp (disable output force)
*/
static int32_t ImmVibeSPI_ForceOut_AmpDisable(u_int8_t nActuatorIndex)
{
	if (g_bampenabled) {
		g_bampenabled = false;
		if (vibrator_drvdata.vib_model == HAPTIC_PWM) {
			gpio_tlmm_config(GPIO_CFG(vibrator_drvdata.\
			vib_pwm_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, \
			GPIO_CFG_2MA), 1);
			gpio_set_value(vibrator_drvdata.vib_pwm_gpio, \
			    VIBRATION_OFF);
			gpio_direction_output(vibrator_drvdata.vib_en_gpio,\
			    VIBRATION_OFF);
		}
		gpio_tlmm_config(GPIO_CFG(vibrator_drvdata.haptic_pwr_en_gpio,\
		0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		if (vibrator_drvdata.is_pmic_haptic_pwr_en)
			gpio_direction_output(vibrator_drvdata.haptic_pwr_en_gpio,1);
		else
			gpio_direction_output(vibrator_drvdata.haptic_pwr_en_gpio,0);
		printk(KERN_DEBUG "tspdrv: %s\n", __func__);
	}

	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
static int32_t ImmVibeSPI_ForceOut_AmpEnable(u_int8_t nActuatorIndex)
{
	if (!g_bampenabled) {
		g_bampenabled = true;
		if (vibrator_drvdata.vib_model == HAPTIC_PWM) {
			gpio_tlmm_config(GPIO_CFG(vibrator_drvdata.\
			vib_pwm_gpio, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, \
			GPIO_CFG_2MA), 1);
			gpio_direction_output(vibrator_drvdata.vib_en_gpio,\
				VIBRATION_ON);
		}
		if (vibrator_drvdata.is_pmic_haptic_pwr_en)
			gpio_direction_output(vibrator_drvdata.haptic_pwr_en_gpio,0);
		else
			gpio_direction_output(vibrator_drvdata.haptic_pwr_en_gpio,1);
		printk(KERN_DEBUG "tspdrv: %s\n", __func__);
	}

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq,
** disable amp, etc...
*/
static int32_t ImmVibeSPI_ForceOut_Initialize(void)
{
	int ret;
	DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Initialize.\n"));
	/* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */
	g_bampenabled = true;

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator
	** (provide the actuator index as input argument).
	*/

	/* set gpio config	*/
	if (vibrator_drvdata.is_pmic_haptic_pwr_en) {
		struct pm_gpio pwr_ena = {
			.direction = PM_GPIO_DIR_OUT,
			.output_buffer = 0,
			.output_value = 0,
			.pull = PM_GPIO_PULL_NO,
			.vin_sel = 2,
			.out_strength = PM_GPIO_STRENGTH_HIGH,
			.function = PM_GPIO_FUNC_1,
			.inv_int_pol = 0,
		};

		ret = gpio_request(vibrator_drvdata.haptic_pwr_en_gpio, \
			"Haptic pwr enable");
		if (ret < 0) {
			printk(KERN_ERR "Haptic enable gpio_request is failed\n");
			goto err1;
		}

		ret = pm8xxx_gpio_config(vibrator_drvdata.haptic_pwr_en_gpio, \
				&pwr_ena);
		if (ret < 0) {
			printk(KERN_ERR "failed to configure Haptic enable pmic gpio\n");
			goto err1;
		}
	} else {
		ret = gpio_request(vibrator_drvdata.haptic_pwr_en_gpio,\
				"Haptic pwr enable");
		if (ret < 0) {
			printk(KERN_ERR "Haptic pwr enable gpio_request is failed\n");
			goto err1;
		}
	}

	if (vibrator_drvdata.vib_model == HAPTIC_PWM) {
		gpio_tlmm_config(GPIO_CFG(vibrator_drvdata.vib_pwm_gpio,  2,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		if (vibrator_drvdata.is_pmic_vib_en) {
			struct pm_gpio vib_ena = {
				.direction = PM_GPIO_DIR_OUT,
				.output_buffer = 0,
				.output_value = 0,
				.pull = PM_GPIO_PULL_NO,
				.vin_sel = 2,
				.out_strength = PM_GPIO_STRENGTH_HIGH,
				.function = PM_GPIO_FUNC_1,
				.inv_int_pol = 0,
			};

			ret = gpio_request(vibrator_drvdata.vib_en_gpio, \
				"vib enable");
			if (ret < 0) {
				printk(KERN_ERR "vib enable gpio_request is failed\n");
				goto err2;
			}

			ret = pm8xxx_gpio_config(vibrator_drvdata.vib_en_gpio, \
					&vib_ena);
			if (ret < 0) {
				printk(KERN_ERR "failed to configure vib enable pmic gpio\n");
				goto err2;
			}
		} else {
			ret = gpio_request(vibrator_drvdata.vib_en_gpio, \
				"vib enable");
			if (ret < 0) {
				printk(KERN_ERR "vib enable gpio_request is failed\n");
				goto err2;
			}
		}
	}
	ImmVibeSPI_ForceOut_AmpDisable(0);
	return VIBE_S_SUCCESS;

err2:
	gpio_free(vibrator_drvdata.haptic_pwr_en_gpio);
err1:
	;
	return VIBE_E_FAIL;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
static int32_t ImmVibeSPI_ForceOut_Terminate(void)
{
	DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Terminate.\n"));

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator
	** (provide the actuator index as input argument).
	*/
	ImmVibeSPI_ForceOut_AmpDisable(0);
	if (vibrator_drvdata.vib_model == HAPTIC_PWM)
		gpio_free(vibrator_drvdata.vib_en_gpio);
	gpio_free(vibrator_drvdata.haptic_pwr_en_gpio);

	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
static int32_t ImmVibeSPI_ForceOut_SetSamples(u_int8_t nActuatorIndex,
						u_int16_t nOutputSignalBitDepth,
						u_int16_t nBufferSizeInBytes,
						int8_t *pForceOutputBuffer)
{
	int8_t nforce;
	static int8_t pre_nforce;
	switch (nOutputSignalBitDepth) {
	case 8:
		/* pForceOutputBuffer is expected to contain 1 byte */
		if (nBufferSizeInBytes != 1) {
			DbgOut(KERN_ERR
			"[ImmVibeSPI] ImmVibeSPI_ForceOut_SetSamples nBufferSizeInBytes =  %d\n",
			nBufferSizeInBytes);
			return VIBE_E_FAIL;
		}
		nforce = pForceOutputBuffer[0];
		break;
	case 16:
		/* pForceOutputBuffer is expected to contain 2 byte */
		if (nBufferSizeInBytes != 2)
			return VIBE_E_FAIL;

		/* Map 16-bit value to 8-bit */
		nforce = ((int16_t *)pForceOutputBuffer)[0] >> 8;
		break;
	default:
		/* Unexpected bit depth */
		return VIBE_E_FAIL;
	}

	if (nforce == 0) {
		/* Set 50% duty cycle or disable amp */
		ImmVibeSPI_ForceOut_AmpDisable(0);
		vibe_pwm_onoff(0);
		nforce = 0;
		pre_nforce = 0;
	} else {
		if (nforce > 0)
			nforce = 127 - nforce;
		if (nforce < 10) { 
			nforce = -127;
//			pr_info("[imosey] nforce'ed to -127\n");
		}
		/* Map force from [-127, 127] to [0, PWM_DUTY_MAX] */
		/* printk(KERN_DEBUG "[tspdrv]nForce===%d\n", nforce); */
		if (pre_nforce != nforce) {
			vibe_pwm_onoff(1);
			vibe_set_pwm_freq(nforce);
			ImmVibeSPI_ForceOut_AmpEnable(0);
			pre_nforce = nforce;
		}
	}
	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
static int32_t ImmVibeSPI_Device_GetName(
		u_int8_t nActuatorIndex, char *szDevName, int nSize)
{
	return VIBE_S_SUCCESS;
}


