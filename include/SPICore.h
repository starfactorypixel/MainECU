#pragma once
#include "driver/spi_master.h"
#include <SPIManager.h>
#include <drivers/SPI_W25Q128JV.h>

namespace SPICore
{

	





	void SPI_OnCFG(const SPIManagerInterface::spi_config_t &config);
	void SPI_OnTx(uint8_t *data, uint16_t length);
	void SPI_OnRx(uint8_t *data, uint16_t length);
	void SPI_OnTxRx(uint8_t *tx_data, uint8_t *rx_data, uint16_t length);


	SPIManager<3> manager(SPI_OnCFG, SPI_OnTx, SPI_OnRx, SPI_OnTxRx);
	SPI_W25Q128JV flash({GPIO_NUM_9}, SPI_MASTER_FREQ_40M);

	spi_device_handle_t spi;

	void SPI_Init()
	{
		spi_bus_config_t buscfg = 
		{
			.mosi_io_num = GPIO_NUM_11,
			.miso_io_num = GPIO_NUM_13,
			.sclk_io_num = GPIO_NUM_12,
			.quadwp_io_num = GPIO_NUM_NC,
			.quadhd_io_num = GPIO_NUM_NC,
			.max_transfer_sz = 4096,
		};

		spi_device_interface_config_t devcfg = 
		{
			.mode = 0,
			.clock_speed_hz = SPI_MASTER_FREQ_40M,
			.spics_io_num = GPIO_NUM_NC,
			.flags = SPI_DEVICE_HALFDUPLEX/* | SPI_DEVICE_BIT_LSBFIRST*/,
			.queue_size = 7,
		};
		
		spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
		spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
	}
	
	void SPI_OnCFG(const SPIManagerInterface::spi_config_t &config)
	{

	}
	
	void SPI_OnTx(uint8_t *data, uint16_t length)
	{
		spi_transaction_t transaction = 
		{
			.length = (length * 8U),
			.tx_buffer = data,
			//.rx_buffer = nullptr,
		};
		spi_device_transmit(spi, &transaction);
	}
	
	void SPI_OnRx(uint8_t *data, uint16_t length)
	{
		spi_transaction_t transaction = 
		{
			.rxlength = (length * 8U),
			.rx_buffer = data,
		};
		spi_device_transmit(spi, &transaction);
	}
	
	void SPI_OnTxRx(uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
	{
		/*
		spi_transaction_t transaction = 
		{
			.length = (length * 8U),
			.tx_buffer = tx_data,
			.rx_buffer = rx_data,
		};
		spi_device_transmit(spi, &transaction);
		*/
	}
	
	inline void Setup()
	{
		SPI_Init();
		manager.AddDevice(flash);

		uint8_t did[3];
		flash.ReadDevID(did);
		DEBUG_LOG_ARRAY_HEX("NOR_dID", did, sizeof(did));
		DEBUG_LOG_NEW_LINE();

		uint8_t uid[8];
		flash.ReadUniqueID(uid);
		DEBUG_LOG_ARRAY_HEX("NOR_uID", uid, sizeof(uid));
		DEBUG_LOG_NEW_LINE();


		return;
	}
	
	inline void Loop(uint32_t &time)
	{
		manager.Tick(time);

		time = millis();
		return;
	}
}
