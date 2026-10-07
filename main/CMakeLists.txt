set(c5vrx_srcs
    "main.c"
)

if(CONFIG_C5VRX_WAVESHARE_LCD_SMOKE_TEST)
    # Display-only hardware bring-up. Do not compile/start RF, PARLIO or
    # BitScrambler paths: the stock IQ GPIO map overlaps the Waveshare LCD/I2C
    # pins and must be resolved before integrated receiver operation.
    list(APPEND c5vrx_srcs
        "waveshare_board_power.c"
        "waveshare_lcd.c"
    )
else()
    list(APPEND c5vrx_srcs
        "board_config.c"
        "arc_phy.c"
        "arc_v3_controller.c"
        "arc_v5_autotune.c"
        "rx_auto_lab.c"
        "rf.c"
        "video.c"
        "direct_gain.c"
        "direct_gain_v2.c"
        "direct_gain_v3.c"
        "analog_video_detect.c"
        "menu_raster.c"
    )

    if(CONFIG_C5VRX_BS_RELATIVE_WORKER_PROBE)
        list(APPEND c5vrx_srcs "bs_relative_worker_probe.c")
    endif()
    if(CONFIG_C5VRX_BS_RELATIVE_MIDDLE_PROBE)
        list(APPEND c5vrx_srcs "bs_relative_middle_probe.c")
    endif()
    if(CONFIG_C5VRX_BS_ADDCTIA_PROBE)
        list(APPEND c5vrx_srcs "bs_addctia_probe.c")
    endif()
    if(CONFIG_C5VRX_PHY_PHASE_TAP_PROBE)
        list(APPEND c5vrx_srcs "phy_phase_tap_probe.c")
    endif()
endif()

idf_component_register(
    SRCS ${c5vrx_srcs}
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_phy
        nvs_flash
        esp_event
        esp_netif
        esp_driver_parlio
        esp_driver_gpio
        esp_driver_bitscrambler
        esp_driver_usb_serial_jtag
        esp_driver_spi
        esp_driver_i2c
        esp_lcd
        vfs
        esp_rom
        esp_mm
        esp_timer
        freertos
        heap
        hal
        esp_hw_support
        esp_driver_gptimer
)

if(NOT CONFIG_C5VRX_WAVESHARE_LCD_SMOKE_TEST)
    # Proven default plus opt-in experimental demod/output paths.
    target_bitscrambler_add_src("fm.bsasm")
    target_bitscrambler_add_src("fm_relative_golden.bsasm")
    target_bitscrambler_add_src("fm_phase5_360.bsasm")
    if(CONFIG_C5VRX_BS_RELATIVE_WORKER_PROBE)
        target_bitscrambler_add_src("bs_relative_worker_probe.bsasm")
    endif()
    if(CONFIG_C5VRX_BS_RELATIVE_MIDDLE_PROBE)
        target_bitscrambler_add_src("bs_relative_middle_probe.bsasm")
    endif()
    target_bitscrambler_add_src("fm_phase5_fsm_capture.bsasm")
    target_bitscrambler_add_src("fm4.bsasm")
    if(CONFIG_C5VRX_BS_ADDCTIA_PROBE)
        target_bitscrambler_add_src("bs_addctia_probe.bsasm")
    endif()
    if(CONFIG_C5VRX_DAC_8BIT)
        target_bitscrambler_add_src("fm_phase8_8bit.bsasm")
    else()
        target_bitscrambler_add_src("fm_phase8_hr_live.bsasm")
    endif()
    target_bitscrambler_add_src("fm_hc.bsasm")
    if(C5VRX4_EXPERIMENT)
        include("${PROJECT_DIR}/component.cmake")
    endif()
endif()
