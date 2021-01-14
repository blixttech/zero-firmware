if(CONFIG_ADC)
    add_subdirectory(adc)
endif()

if(CONFIG_ADC_TRIGGER)
    add_subdirectory(adc_trigger)
endif()

if(CONFIG_COUNTER)
    add_subdirectory(counter)
endif()

if(CONFIG_DMA)
    add_subdirectory(dma)
endif()

if(CONFIG_EEPROM)
    add_subdirectory(eeprom)
endif()

if(CONFIG_INPUT_CAPTURE)
    add_subdirectory(input_capture)
endif()

