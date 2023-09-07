if(CONFIG_ADC)
    add_subdirectory(adc)
endif()

if(CONFIG_ADC_TRIGGER)
    add_subdirectory(adc_trigger)
endif()

if(CONFIG_DMA)
    add_subdirectory(dma)
endif()

if(CONFIG_SICI)
    add_subdirectory(sici)
endif()
