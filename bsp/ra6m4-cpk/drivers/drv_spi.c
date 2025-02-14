/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-08-23     Mr.Tiger     first version
 */
/**< Note : Turn on any DMA mode and all SPIs will turn on DMA */

#include "drv_spi.h"

#ifdef RT_USING_SPI

//#define DRV_DEBUG
#define DBG_TAG              "drv.spi"
#ifdef DRV_DEBUG
    #define DBG_LVL               DBG_LOG
#else
    #define DBG_LVL               DBG_INFO
#endif /* DRV_DEBUG */
#include <rtdbg.h>

static struct ra_spi_handle spi_handle[] =
{
#ifdef BSP_USING_SPI0
    {.bus_name = "spi0", .spi_ctrl_t = &g_spi0_ctrl, .spi_cfg_t = &g_spi0_cfg,},
#endif

#ifdef BSP_USING_SPI1
    {.bus_name = "spi1", .spi_ctrl_t = &g_spi1_ctrl, .spi_cfg_t = &g_spi1_cfg,},
#endif
};

static struct ra_spi spi_config[sizeof(spi_handle) / sizeof(spi_handle[0])] = {0};

void g_spi0_callback(spi_callback_args_t *p_args)
{
    rt_interrupt_enter();
    if (SPI_EVENT_TRANSFER_COMPLETE == p_args->event)
    {
        LOG_D("SPI0 cb");
    }
    rt_interrupt_leave();
}

void g_spi1_callback(spi_callback_args_t *p_args)
{
    rt_interrupt_enter();
    if (SPI_EVENT_TRANSFER_COMPLETE == p_args->event)
    {
        LOG_D("SPI1 cb");
    }
    rt_interrupt_leave();
}

static rt_err_t ra_write_message(struct rt_spi_device *device, const void *send_buf, const rt_size_t len)
{
    RT_ASSERT(device != NULL);
    RT_ASSERT(device->parent.user_data != NULL);
    RT_ASSERT(send_buf != NULL);
    RT_ASSERT(len > 0);
    rt_err_t err = RT_EOK;
    struct ra_spi *spi_dev =  rt_container_of(device->bus, struct ra_spi, bus);
    spi_dev->cs_pin = *(rt_uint32_t *)device->parent.user_data;

    /**< Configure Select Line */
    R_BSP_PinWrite(spi_dev->cs_pin, BSP_IO_LEVEL_HIGH);

    /* Start a write transfer */
    R_BSP_PinWrite(spi_dev->cs_pin, BSP_IO_LEVEL_LOW);

    /**< send msessage */
    err = R_SPI_Write((spi_ctrl_t *)spi_dev->ra_spi_handle_t->spi_ctrl_t, send_buf, len, spi_dev->rt_spi_cfg_t->data_width);
    if (RT_EOK != err)
    {
        LOG_E("%s write failed.", spi_dev->ra_spi_handle_t->bus_name);
        return -RT_ERROR;
    }

    return len;
}

static rt_err_t ra_read_message(struct rt_spi_device *device, void *recv_buf, const rt_size_t len)
{
    RT_ASSERT(device != NULL);
    RT_ASSERT(device->parent.user_data != NULL);
    RT_ASSERT(recv_buf != NULL);
    RT_ASSERT(len > 0);
    rt_err_t err = RT_EOK;
    struct ra_spi *spi_dev =  rt_container_of(device->bus, struct ra_spi, bus);
    spi_dev->cs_pin = *(rt_uint32_t *)device->parent.user_data;

    /**< Configure Select Line */
    R_BSP_PinWrite(spi_dev->cs_pin, BSP_IO_LEVEL_HIGH);

    /* Start read transfer */
    R_BSP_PinWrite(spi_dev->cs_pin, BSP_IO_LEVEL_LOW);

    /**< receive message */
    err = R_SPI_Read((spi_ctrl_t *)spi_dev->ra_spi_handle_t->spi_ctrl_t, recv_buf, len, spi_dev->rt_spi_cfg_t->data_width);
    if (RT_EOK != err)
    {
        LOG_E("\n%s write failed.\n", spi_dev->ra_spi_handle_t->bus_name);
        return -RT_ERROR;
    }

    return len;
}

static rt_err_t ra_write_read_message(struct rt_spi_device *device, struct rt_spi_message *message)
{
    RT_ASSERT(device != NULL);
    RT_ASSERT(message != NULL);
    RT_ASSERT(message->length > 0);
    rt_err_t err = RT_EOK;
    struct ra_spi *spi_dev =  rt_container_of(device->bus, struct ra_spi, bus);

    /**< write and receive message */
    err = R_SPI_WriteRead((spi_ctrl_t *)spi_dev->ra_spi_handle_t->spi_ctrl_t, message->send_buf, message->recv_buf, message->length, spi_dev->rt_spi_cfg_t->data_width);
    if (RT_EOK != err)
    {
        LOG_E("%s write and read failed.", spi_dev->ra_spi_handle_t->bus_name);
        return -RT_ERROR;
    }

    return message->length;
}

/**< init spi TODO : MSB does not support modification */
static rt_err_t ra_hw_spi_configure(struct rt_spi_device *device,
                                    struct rt_spi_configuration *configuration)
{
    RT_ASSERT(device != NULL);
    RT_ASSERT(configuration != NULL);
    rt_err_t err = RT_EOK;

    struct ra_spi *spi_dev =  rt_container_of(device->bus, struct ra_spi, bus);
    spi_dev->cs_pin = (rt_uint32_t)device->parent.user_data;

    /**< data_width : 1 -> 8 bits , 2 -> 16 bits, 4 -> 32 bits, default 32 bits*/
    rt_uint8_t data_width = configuration->data_width / 8;
    RT_ASSERT(data_width == 1 || data_width == 2 || data_width == 4);
    configuration->data_width = configuration->data_width / 8;
    spi_dev->rt_spi_cfg_t = configuration;

    spi_extended_cfg_t *spi_cfg = (spi_extended_cfg_t *)spi_dev->ra_spi_handle_t->spi_cfg_t->p_extend;

    /**< Configure Select Line */
    R_BSP_PinWrite(spi_dev->cs_pin, BSP_IO_LEVEL_HIGH);

    /**< config bitrate */
    R_SPI_CalculateBitrate(spi_dev->rt_spi_cfg_t->max_hz, &spi_cfg->spck_div);

    /**< init */
    err = R_SPI_Open((spi_ctrl_t *)spi_dev->ra_spi_handle_t->spi_ctrl_t, (spi_cfg_t const * const)spi_dev->ra_spi_handle_t->spi_cfg_t);
    /* handle error */
    if (RT_EOK != err)
    {
        LOG_E("%s init failed.", spi_dev->ra_spi_handle_t->bus_name);
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_uint32_t ra_spixfer(struct rt_spi_device *device, struct rt_spi_message *message)
{
    RT_ASSERT(device != NULL);
    RT_ASSERT(message != NULL);
    rt_err_t err = RT_EOK;

    if (message->length <= 0)
    {
        LOG_E("buf length err.");
    }
    else
    {
        if (message->send_buf == RT_NULL && message->recv_buf != RT_NULL)
        {
            /**< receive message */
            err = ra_read_message(device, (void *)message->recv_buf, (const rt_size_t)message->length);
        }
        else if (message->send_buf != RT_NULL && message->recv_buf == RT_NULL)
        {
            /**< send message */
            err = ra_write_message(device, (const void *)message->send_buf, (const rt_size_t)message->length);
        }
        else if (message->send_buf != RT_NULL && message->recv_buf != RT_NULL)
        {
            /**< send and receive message */
            err =  ra_write_read_message(device, message);
        }
    }
    return err;
}

static const struct rt_spi_ops ra_spi_ops =
{
    .configure = ra_hw_spi_configure,
    .xfer = ra_spixfer,
};

void rt_hw_spi_device_attach(struct rt_spi_device *device, const char *device_name, const char *bus_name, void *user_data)
{
    RT_ASSERT(device != NULL);
    RT_ASSERT(device_name != NULL);
    RT_ASSERT(bus_name != NULL);
    RT_ASSERT(user_data != NULL);

    rt_err_t err = rt_spi_bus_attach_device(device, device_name, bus_name, user_data);
    if (RT_EOK != err)
    {
        LOG_E("%s attach failed.", bus_name);
    }
}

int ra_hw_spi_init(void)
{
    for (rt_uint8_t spi_index = 0; spi_index < sizeof(spi_handle) / sizeof(spi_handle[0]); spi_index++)
    {
        spi_config[spi_index].ra_spi_handle_t = &spi_handle[spi_index];

        /**< register spi bus */
        rt_err_t err = rt_spi_bus_register(&spi_config[spi_index].bus, spi_handle[spi_index].bus_name, &ra_spi_ops);
        if (RT_EOK != err)
        {
            LOG_E("%s bus register failed.", spi_config[spi_index].ra_spi_handle_t->bus_name);
        }
    }

    return RT_EOK;
}
INIT_BOARD_EXPORT(ra_hw_spi_init);
#endif /* RT_USING_SPI */
