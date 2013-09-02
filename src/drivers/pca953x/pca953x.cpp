/****************************************************************************
 *
 *   Copyright (C) 2012, 2013 TMR Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name TMR nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file pca953x.cpp
 *
 * Driver for the onboard NXP PCA9533 4-bit I2C-bus LED dimmer  
 * and PCA9536 4-bit I2C-bus and SMBus I/O port connected via I2C.
 *
 */

#include <nuttx/config.h>

#include <drivers/device/i2c.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>

#include <board_config.h>

#include <systemlib/perf_counter.h>
#include <systemlib/err.h>

#include <drivers/drv_led.h>
#include <drivers/drv_gpio.h>
#include <drivers/drv_hrt.h>

#include <uORB/uORB.h>
#include <uORB/topics/subsystem_info.h>

#include <float.h>

/* PCA0533 4-bit I2C-bus LED dimmer */
#define PCA9533_ADDRESS        TMR_I2C_OBDEV_PCA9533

#define PCA9533_REG_START      0x00
#define PCA9533_REG_INPUT      0x00
#define PCA9533_REG_PSC0       0x01
#define PCA9533_REG_PWM0       0x02
#define PCA9533_REG_PSC1       0x03
#define PCA9533_REG_PWM1       0x04
#define PCA9533_REG_LS0        0x05

#define PCA9533_LED_OFF        0x00
#define PCA9533_LED_ON         0x01
#define PCA9533_LED_PWM0       0x02
#define PCA9533_LED_PWM1       0x03

#define PCA9533_AI_FLAG        0x10

#define PCA9533_LED0           0x01
#define PCA9533_LED1           0x02
#define PCA9533_LED2           0x04
#define PCA9533_LED3           0x08

#define LED_BLINK_1HZ          500
#define LED_BLINK_10HZ         200
#define LED_BLINK_20HZ         100

#define MIN_MSEC 7 // 7 msec for minimal
#define MAX_MSEC 1684 // 1684 msec for maximal

struct pca9533_bit_t
{
    uint8_t input : 8 ;
    uint8_t psc0  : 8 ;
    uint8_t pwm0  : 8 ;
    uint8_t psc1  : 8 ;
    uint8_t pwm1  : 8 ;
    
    struct _ls0
    {
    	uint8_t led0 : 2 ;
        uint8_t led1 : 2 ;
       	uint8_t led2 : 2 ;
        uint8_t led3 : 2 ;
    }ls0;

};

struct pca9533_t
{
    uint8_t input : 4 ;
	uint8_t       : 4 ;
    uint8_t psc0  : 8 ;
    uint8_t pwm0  : 8 ;
    uint8_t psc1  : 8 ;
    uint8_t pwm1  : 8 ;
	uint8_t ls0   : 8 ;
    

};

/* PCA0536 4-bit I2C-bus and SMBus I/O port */
#define PCA9536_ADDRESS        TMR_I2C_OBDEV_PCA9536

#define PCA9536_REG_START      0x00
#define PCA9536_REG_INPUT      0x00
#define PCA9536_REG_OUTPUT     0x01
#define PCA9536_REG_POLARITY   0x02
#define PCA9536_REG_CONFIG     0x03

#define PCA9536_IO0 0x01
#define PCA9536_IO1 0x02
#define PCA9536_IO2 0x04
#define PCA9536_IO3 0x08

#define PCA9536_IO_O 0x00
#define PCA9536_IO_I 0x01

struct pca9536_bit_t
{

   	struct _input
   	{
   		uint8_t ix0 : 1 ;
    	uint8_t ix1 : 1 ;
    	uint8_t ix2 : 1 ;
    	uint8_t ix3 : 1 ;
    }input;


    struct _output
    {
    	uint8_t ox0 : 1 ;
    	uint8_t ox1 : 1 ;
    	uint8_t ox2 : 1 ;
    	uint8_t ox3 : 1 ;
   	}output;


   	struct _polarity
    {
    	uint8_t nx0 : 1 ;
    	uint8_t nx1 : 1 ;
    	uint8_t nx2 : 1 ;
    	uint8_t nx3 : 1 ;
    }polarity;

    struct _config
    {
    	uint8_t cx0 : 1 ;
    	uint8_t cx1 : 1 ;
    	uint8_t cx2 : 1 ;
    	uint8_t cx3 : 1 ;
    }config;
};

struct pca9536_t
{
   	uint8_t input    : 4 ;
	uint8_t          : 4 ;
	uint8_t output   : 4 ;
	uint8_t          : 4 ;
	uint8_t polarity : 4 ;
	uint8_t          : 4 ;
	uint8_t config   : 4 ;
	uint8_t          : 4 ;
};

struct pca_tbl_t
{
    pca9533_t* pca9533;
    pca9536_t* pca9536;
};

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

#ifndef CONFIG_SCHED_WORKQUEUE
# error This requires CONFIG_SCHED_WORKQUEUE.
#endif

class PCA953X : public device::I2C
{
public:
	PCA953X(int bus);
	virtual ~PCA953X();
	virtual int		init();
	virtual int		ioctl(struct file *filp, int cmd, unsigned long arg);

	/**
	 * Diagnostics - print some basic information about the driver.
	 */
	void			print_info();

protected:
	virtual int		probe();

private:
	work_s			_work;

	pca9533_t _pca9533;
    pca9536_t _pca9536;

	int			_bus;			/**< the bus the device is connected to */

    /**
	 * Initialise the automatic measurement state machine and start it.
	 *
	 * @note This function is called at open and error time.  It might make sense
	 *       to make it more aggressive about resetting the bus in case of errors.
	 */
	void			start();

	/**
	 * Stop the automatic measurement state machine.
	 */
	void			stop();

	/**
	 * Reset the device
	 */
	int			reset();

    uint8_t pca953x_update(uint8_t pca_id);
    uint8_t pca9533_set_peroid(uint8_t psc, uint32_t msec);
    uint8_t pca9533_set_pwm(uint8_t pwm, uint32_t duty);
    uint8_t pca9533_set_led(uint8_t led, uint32_t mode);
    uint8_t pca9536_config_io(uint8_t io, uint8_t set);

	/**
	 * Write a register.
	 *
	 * @param reg		The register to write.
	 * @param val		The value to write.
	 * @return		OK on write success.
	 */
	int			write_reg(uint8_t reg, uint8_t val);

	/**
	 * Read a register.
	 *
	 * @param reg		The register to read.
	 * @param val		The value read.
	 * @return		OK on read success.
	 */
	int			read_reg(uint8_t reg, uint8_t &val);

};


uint8_t
PCA953X::pca953x_update(uint8_t address)
{
    uint8_t rc = false;

    if(address == OK)
    {
		set_address(PCA9533_ADDRESS);

		transfer((uint8_t *)&_pca9533, sizeof(_pca9533)/sizeof(uint8_t), nullptr, 0);
		if (OK != rc)
		    goto cleanup;
    }
    else if(address == PCA9536_ADDRESS)
    {
		set_address(PCA9536_ADDRESS);

		transfer((uint8_t *)&_pca9536, sizeof(_pca9536)/sizeof(uint8_t), nullptr, 0);
		if (OK != rc)
		    goto cleanup;
		
    }

    rc = true;

cleanup:
    return rc;
}

uint8_t
PCA953X::pca9533_set_peroid(uint8_t psc, uint32_t msec)
{
    uint8_t rc = 0, data = 0;

    if(msec < MIN_MSEC) // min_msev
        data = 0;

    if(msec > MAX_MSEC)
        data = 255;

    data = (uint8_t)((float)msec * 0.152f) - 1;

	set_address(PCA9533_ADDRESS);

    if(psc == PCA9533_REG_PSC0)
    {
		rc = write_reg(PCA9533_REG_PSC0, _pca9533.psc0);

	    if (OK != rc)
		    goto cleanup;

        // save old psc0
        rc = _pca9533.psc0;
        // update table
        _pca9533.psc0 = data;
    }
    
    if(psc == PCA9533_REG_PSC1)
    {
		rc = write_reg(PCA9533_REG_PSC1, _pca9533.psc1);

	    if (OK != rc)
		    goto cleanup;
		

        // save old psc1
        rc = _pca9533.psc1;
        // update table
        _pca9533.psc1 = data;
    }

cleanup:
    return rc;
}

uint8_t
PCA953X::pca9533_set_pwm(uint8_t pwm, uint32_t duty)
{
    uint8_t rc = false;
    int8_t data = 0;

    if(duty < 1)
        data = 0;

    if(duty > 100)
        data = 255;

    data = (uint8_t)(((float)duty/100.0f)*256.0f);

	set_address(PCA9533_ADDRESS);

    if(pwm == PCA9533_REG_PWM0)
    {
		rc = write_reg(PCA9533_REG_PWM0, _pca9533.pwm0);

	    if (OK != rc)
		    goto cleanup;

        // update table
        _pca9533.pwm0 = data;
    }
    
    if(pwm == PCA9533_REG_PWM1)
    {
		rc = write_reg(PCA9533_REG_PWM1, _pca9533.pwm1);

	    if (OK != rc)
		    goto cleanup;

        // update table
        _pca9533.pwm1 = data;
    }

cleanup:
    return rc;
}

uint8_t
PCA953X::pca9533_set_led(uint8_t led, uint32_t mode)
{
    uint8_t rc = OK, val;

	pca9533_bit_t* b = (pca9533_bit_t*)&_pca9533;

	set_address(PCA9533_ADDRESS);

    if((led & PCA9533_LED0) == PCA9533_LED0)
        b->ls0.led0= mode;
    if((led & PCA9533_LED1) == PCA9533_LED1)
        b->ls0.led1 = mode;
    if((led & PCA9533_LED2) == PCA9533_LED2)
        b->ls0.led2 = mode;
    if((led & PCA9533_LED3) == PCA9533_LED3)
        b->ls0.led3 = mode;

    val = _pca9533.ls0;
	rc = write_reg(PCA9533_REG_LS0, val);

	if (OK != rc)
		goto cleanup;

cleanup:
    return rc;
}

uint8_t
PCA953X::pca9536_config_io(uint8_t io, uint8_t set)
{
    uint8_t rc = OK, val;

	pca9536_bit_t* b = (pca9536_bit_t*)&_pca9536;

	set_address(PCA9536_ADDRESS);

    if((io & PCA9536_IO0) == PCA9536_IO0)
        b->config.cx0 = set;
    if((io & PCA9536_IO1) == PCA9536_IO1)
        b->config.cx1 = set;
    if((io & PCA9536_IO2) == PCA9536_IO2)
        b->config.cx2 = set;
    if((io & PCA9536_IO3) == PCA9536_IO3)
        b->config.cx3 = set;

    val = _pca9536.config;
	rc = write_reg(PCA9536_REG_CONFIG, val);

	if (OK != rc)
		goto cleanup;

cleanup:
    return rc;
}



/* for now, we only support one PCA953X */
namespace
{
	PCA953X *g_pca953x;
}


extern "C" __EXPORT int pca953x_main(int argc, char *argv[]);

PCA953X::PCA953X(int bus) :
	I2C("PCA953X", PCA953X_DEVICE_PATH, bus, PCA9533_ADDRESS, 400000),
	_bus(bus)
{
    // enable debug() calls
	_debug_enabled = false;

	memset(&_work, 0, sizeof(_work));
}

PCA953X::~PCA953X()
{
}

int
PCA953X::init()
{
    int ret = ERROR;

	/* do I2C init (and probe) first */
	if (I2C::init() != OK)
		goto out;

    pca9533_set_led(PCA9533_LED0|
		            PCA9533_LED1|
		            PCA9533_LED2|
		            PCA9533_LED3, PCA9533_LED_OFF);

	ret = OK;

out:
	return ret;
}

int
PCA953X::probe()
{
    // Assume the device is too stupid to be discoverable.
	return OK;
}

int
PCA953X::ioctl(struct file *filp, int cmd, unsigned long arg)
{
	int ret = ENOTTY;
	switch (cmd) {

	default:
		break;
	}

	return ret;
}

void
PCA953X::start()
{

}

void
PCA953X::stop()
{

}

int
PCA953X::reset()
{
	return OK;
}

void
PCA953X::print_info()
{
	errx(0, "print_info()");
}

int
PCA953X::write_reg(uint8_t reg, uint8_t val)
{
	uint8_t cmd[] = { reg, val };

	return transfer(&cmd[0], 2, nullptr, 0);
}

int
PCA953X::read_reg(uint8_t reg, uint8_t &val)
{
	return transfer(&reg, 1, &val, 1);
}

/**
 * Local functions in support of the shell command.
 */
namespace pca953x
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
const int ERROR = -1;

PCA953X	*g_dev;

void	start();
void	test();
void	reset();
void	info();

/**
 * Start the driver.
 */
void
start()
{
	int fd;

	if (g_dev != nullptr)
		/* if already started, the still command succeeded */
		errx(0, "already started");

	/* create the driver, attempt expansion bus first */
	g_dev = new PCA953X(TMR_I2C_BUS_EXPANSION);

	if (g_dev != nullptr && OK != g_dev->init()) {
		delete g_dev;
		g_dev = nullptr;
	}

	/* if this failed, attempt onboard sensor */
	if (g_dev == nullptr) {
		g_dev = new PCA953X(TMR_I2C_BUS_ONBOARD);
		if (g_dev != nullptr && OK != g_dev->init()) {
			goto fail;
		}
	}

	if (g_dev == nullptr)
		goto fail;

	/* set the poll rate to default, starts automatic data collection */
	fd = open(PCA953X_DEVICE_PATH, O_RDONLY);

	exit(0);

fail:

	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;
	}

	errx(1, "driver start failed");
}

/**
 * Perform some basic functional tests on the driver;
 * make sure we can collect data from the sensor in polled
 * and automatic modes.
 */
void
test()
{
	errx(0, "test()");
}

/**
 * Reset the driver.
 */
void
reset()
{
	int fd = open(PCA953X_DEVICE_PATH, O_RDONLY);

	if (fd < 0)
		err(1, "failed ");

	exit(0);
}

/**
 * Print a little info about the driver.
 */
void
info()
{
	if (g_dev == nullptr)
		errx(1, "driver not running");

	g_dev->print_info();

	exit(0);
}

} // namespace


int
pca953x_main(int argc, char *argv[])
{
	/*
	 * Start/load the driver.
	 */
	if (!strcmp(argv[1], "start"))
		pca953x::start();

	/*
	 * Test the driver/device.
	 */
	if (!strcmp(argv[1], "test"))
		pca953x::test();

	/*
	 * Reset the driver.
	 */
	if (!strcmp(argv[1], "reset"))
		pca953x::reset();

	/*
	 * Print driver information.
	 */
	if (!strcmp(argv[1], "info") || !strcmp(argv[1], "status"))
		pca953x::info();

    errx(1, "unrecognized command, try 'start', 'test', 'reset' or 'info'");

}
