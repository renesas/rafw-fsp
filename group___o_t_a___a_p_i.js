var group___o_t_a___a_p_i =
[
    [ "rm_ota_w_update_proc_t", "group___o_t_a___a_p_i.html#structrm__ota__w__update__proc__t", [
      [ "update_type", "group___o_t_a___a_p_i.html#acb52bbc6c6c2018e638168ecdd386ef9", null ],
      [ "update_cache", "group___o_t_a___a_p_i.html#ac2ebda6cf1b5ec0b247dae54491603c6", null ],
      [ "auto_swap", "group___o_t_a___a_p_i.html#a3e984f340952562e2202abde9a43eda3", null ],
      [ "download_sflash_addr", "group___o_t_a___a_p_i.html#a512157480e114a21fb6d21ce181d0598", null ],
      [ "download_notify", "group___o_t_a___a_p_i.html#ae37893485ce70b55dcd4aacf925a1fd3", null ],
      [ "swap_notify", "group___o_t_a___a_p_i.html#a3f209221bdd7c87b71ce6ca784e7e613", null ],
      [ "update_state", "group___o_t_a___a_p_i.html#ae1f50152e62e891e77ca88f06e5a0c6c", null ],
      [ "status", "group___o_t_a___a_p_i.html#a173439ec53b17f00c8bf4dd67818d0a3", null ],
      [ "progress_rtos", "group___o_t_a___a_p_i.html#a9119e902dbb6053e287ddca96546a75f", null ],
      [ "progress_mcu_fw", "group___o_t_a___a_p_i.html#aad95908e3d057a8a3945c7ae9a6b4378", null ],
      [ "progress_cert_key", "group___o_t_a___a_p_i.html#aee1f594db08938ce9e06e17fffaeca6c", null ]
    ] ],
    [ "rm_ota_w_image_header_data_t", "group___o_t_a___a_p_i.html#structrm__ota__w__image__header__data__t", null ],
    [ "ota_regions_t", "group___o_t_a___a_p_i.html#structota__regions__t", [
      [ "num_regions", "group___o_t_a___a_p_i.html#a2e4ddad8eb73a4eb21370d1c9e531ea6", null ],
      [ "p_block_array", "group___o_t_a___a_p_i.html#a6fccea2500e51d68609a8a8396f64456", null ]
    ] ],
    [ "ota_info_t", "group___o_t_a___a_p_i.html#structota__info__t", [
      [ "code_ota", "group___o_t_a___a_p_i.html#a364ed37117c3193e88df9f5399ac761a", null ],
      [ "data_ota", "group___o_t_a___a_p_i.html#a23e967bcf27a2a5deb78a79374ff7362", null ]
    ] ],
    [ "ota_callback_args_t", "group___o_t_a___a_p_i.html#structota__callback__args__t", [
      [ "event", "group___o_t_a___a_p_i.html#a5adca5db6520ddb1116eb7fced72860d", null ],
      [ "p_context", "group___o_t_a___a_p_i.html#adb0a3d34cf732b333e88a7fb5f122597", null ]
    ] ],
    [ "ota_cfg_t", "group___o_t_a___a_p_i.html#structota__cfg__t", [
      [ "data_ota_bgo", "group___o_t_a___a_p_i.html#a701c000d911e8f0e47aa5dfc6024a9e0", null ],
      [ "p_callback", "group___o_t_a___a_p_i.html#ac0745930fd73a21c4dc1808ada68bfec", null ],
      [ "p_spi_flash", "group___o_t_a___a_p_i.html#a07feb0b2dad768c92e9a97c096c3abde", null ],
      [ "p_extend", "group___o_t_a___a_p_i.html#a1284a59ddcb8080b2ffa6d7ac68ecd81", null ],
      [ "p_context", "group___o_t_a___a_p_i.html#a24ea0cd6d28f0242c93a1d603c04eaf6", null ],
      [ "ipl", "group___o_t_a___a_p_i.html#abc31d173cd1c7f5a7261a62d3c293edb", null ],
      [ "irq", "group___o_t_a___a_p_i.html#a917efb555c35952be9986f26e8b9517d", null ],
      [ "err_ipl", "group___o_t_a___a_p_i.html#a38414a8ebc061b1d190ed1ca89ec19cb", null ],
      [ "err_irq", "group___o_t_a___a_p_i.html#adb034f91d66aa7070d69fa5f62f4d997", null ]
    ] ],
    [ "ota_api_t", "group___o_t_a___a_p_i.html#structota__api__t", [
      [ "open", "group___o_t_a___a_p_i.html#a1e98ab810178c98d0aada0d0c6554678", null ],
      [ "swap", "group___o_t_a___a_p_i.html#a419490394ca8f8a21f1f2247a3c75163", null ],
      [ "getImageInfo", "group___o_t_a___a_p_i.html#a6f1e6e3fa0ea15c13649f9a05db70f92", null ],
      [ "bootIdxSet", "group___o_t_a___a_p_i.html#a330f0dd8d090a14c3c12483a66254665", null ],
      [ "bootIdxGet", "group___o_t_a___a_p_i.html#ab73d28993704fd3b48294a196d356d05", null ],
      [ "getAddr", "group___o_t_a___a_p_i.html#aaed12d778a0b3b40133f35cd8bc11001", null ],
      [ "setAddr", "group___o_t_a___a_p_i.html#ad61a88b8661bcdb37ac36ff3e9d3a7b0", null ],
      [ "cert", "group___o_t_a___a_p_i.html#ab4e48475b326bd3f90e9a73f33a4034c", null ],
      [ "close", "group___o_t_a___a_p_i.html#a9333cfa6763e0e2798e57a0f864abead", null ]
    ] ],
    [ "ota_instance_t", "group___o_t_a___a_p_i.html#structota__instance__t", [
      [ "p_ctrl", "group___o_t_a___a_p_i.html#a6307aa8a780f6f7fd1649b58d0a152b2", null ],
      [ "p_cfg", "group___o_t_a___a_p_i.html#ab9ffbc02d2f72b0b638a377dd0ef21cf", null ],
      [ "p_api", "group___o_t_a___a_p_i.html#a94041da105d52f84ccb8de5b52014673", null ]
    ] ],
    [ "ota_ctrl_t", "group___o_t_a___a_p_i.html#ga02dc241703d79917f2f118d4ad092b60", null ],
    [ "rm_ota_w_update_type_t", "group___o_t_a___a_p_i.html#gaf2dae2280163e181de63894b8f165a71", [
      [ "RM_OTA_W_TYPE_INIT", "group___o_t_a___a_p_i.html#ggaf2dae2280163e181de63894b8f165a71a4e7218813d9ae38299322531194f4a9b", null ],
      [ "RM_OTA_W_TYPE_RTOS", "group___o_t_a___a_p_i.html#ggaf2dae2280163e181de63894b8f165a71a09b8c502b213fa64cc9b35e68554ce8e", null ],
      [ "RM_OTA_W_TYPE_BLE_FW", "group___o_t_a___a_p_i.html#ggaf2dae2280163e181de63894b8f165a71a22787d32a3accd0c05762bdaca3b0465", null ],
      [ "RM_OTA_W_TYPE_BLE_COMBO", "group___o_t_a___a_p_i.html#ggaf2dae2280163e181de63894b8f165a71a8fc8a015c10ba86f91b685db17014bc0", null ],
      [ "RM_OTA_W_TYPE_MCU_FW", "group___o_t_a___a_p_i.html#ggaf2dae2280163e181de63894b8f165a71aba76e96a8113ce75f3fb08b331a1b8f8", null ],
      [ "RM_OTA_W_TYPE_CERT_KEY", "group___o_t_a___a_p_i.html#ggaf2dae2280163e181de63894b8f165a71a6ec9a35b3769a74adf95f98489caea77", null ]
    ] ],
    [ "ota_event_t", "group___o_t_a___a_p_i.html#gae8b73934da2b137d24d46ed7dbdaea31", [
      [ "RM_OTA_W_DOWNLOAD_RESULT_OK", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31ac556e0a60b20936958a946e2d313a3f1", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_UNKNOWN", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31a55f18b01475dcf61957f9389e208ad8f", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_CONNECT", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31ad701f3c91b4c9420369a73f6f7b7557a", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_HOSTNAME", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31ac228e7ea5d47df70a98f356e26902067", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_CLOSED", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31a9cb66e5af3f6b2baea326ada53b76f40", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_TIMEOUT", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31a5635b9b200b60fff48935be5c74ffb89", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_SVR_RESP", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31acb57cf765031c044c7e10c3c2316c27f", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_MEM", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31aa55b86cb04ed0304e5e5759692e6ddd5", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_LOCAL_ABORT", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31afaeea6506bc9d424ee63cadb76756cec", null ],
      [ "RM_OTA_W_DOWNLOAD_RESULT_ERR_CONTENT_LEN", "group___o_t_a___a_p_i.html#ggae8b73934da2b137d24d46ed7dbdaea31a47dfcbd0244e522964e8428bced43d14", null ]
    ] ]
];