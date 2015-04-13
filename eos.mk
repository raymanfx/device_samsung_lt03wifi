$(call inherit-product, device/samsung/lt03wifi/full_lt03wifi.mk)

# Inherit some common EOS stuff.
$(call inherit-product, vendor/eos/config/common_full_tablet_wifionly.mk)

PRODUCT_NAME := eos_lt03wifi
PRODUCT_DEVICE := lt03wifi

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_MODEL=SM-P600 \
    PRODUCT_NAME=lt03wifi \
    PRODUCT_DEVICE=lt03wifi \
    TARGET_DEVICE=lt03wifi \
