#ifndef DIAG_IPU_159__1_H
#define DIAG_IPU_159__1_H

#include <stdint.h>

#define SYS_DIAG_PACKET_TYPE_DIAG 0xdd          //!< typ paketu: diagnosticka data
#define SYS_DIAG_PACKET_VER_DIAG  0x0000        //!< verze paketu

#define SYS_DIAG_RESPONSE_DATA_FORMAT_STD 0x11  //!< typ diagnostickych dat: standardni
#define SYS_DIAG_RESPONSE_DATA_FORMAT_EXT 0x22  //!< typ diagnostickych dat: rozsirena

//Masky sw statusu
#define SW_STAT_PIP_MASK   0x01         //!< maska podle ktere se promitne status vlakna s gst pipeline do swStatus v diag. packetu, bublina: gst pip. thread
#define SW_STAT_SERV_MASK     0x02      //!< maska podle ktere se promitne status vlakna se serverem do swStatus v diag. packetu, bublina: main thread
#define SW_STAT_UDP_CLI_MASK    0x04    //!< maska podle ktere se promitne status vlakna s udp clientem do swStatus v diag. packetu, bublina: diag. thread
#define SW_STAT_KALIB_MASK      0x08    //!< maska podle ktere se promitne status vlakna kalibrace do swStatus v diag. packetu, bublina: calib. thread
#define SW_STAT_BITRATE_WARN_MASK    0x10    //!< maska varovani kvuli vysoke bitrate
#define SW_STAT_BITRATE_ERR_MASK     0x20    //!< maska chyby prilis vysoka vystupni bitrate, bude se omezovat

//Masky hw statusu
#define HW_STAT_TC358743_TIM_MASK   0x01    //!< maska pro status TC358743 do hwStatus v diag. packetu, bublina: tc358743 input
#define HW_STAT_TC358743_TRANS_MODE_MASK    0x02    //!< maska pro stav transmit mode TC358743 do hwStatus v diag. packetu, bublina: tc358743 transmit
#define HW_STAT_BASLER_INIT_MASK	0x04		//!< maska pro uspech/neuspech prepsani registru kamery pro full frame atp., bublina: camera init
#define HW_STAT_FPD_LINK_INIT_MASK  0x08        //!< maska pro uspech/neuspech inicializace FPDIII linky, bublina: FPD III link init
#define HW_STAT_SN75DP159_MASK		0x10		//!< maska pro kontrolu io sn75dp159, bublina: SN75DP159 status
#define HW_STAT_DS90UB954_MASK      0x20        //!< maska pro status ds90ub954, bublina: DS90UB954 status
#define HW_STAT_DS90UB953_MASK      0x40        //!< maska pro status ds90ub953, bublina: DS90UB953 status

//Masky pro cteni z UARTU (diagnostika z DSP)
#define UART_MESSAGE_TYPE_ERR_MASK  0x01        //!< maska chyby cteni z uartu (nesedi typ paketu)
#define UART_MESSAGE_CRC_ERR_MASK   0x02		//!< maska chyby cteni z uartu (nesedi crc)
#define UART_MESSAGE_LEN_ERR_MASK   0x04		//!< maska chyby cteni z uartu (nesedi delka)
#define UART_OPEN_ERR_MASK          0x10		//!< maska chyby otevreni uartu
#define UART_GET_ATTR_ERR_MASK      0x20		//!< maska chyby pri cteni atributu uartu
#define UART_SET_ATTR_ERR_MASK      0x40		//!< maska chyby pri nastaveni atributu uartu

#define SYSTEM_ID   50

#define FIRMWARE_NAME   "IPU-159-01_CPU_1.8"      //!< verze softwaru, max 23 znaku !!!

typedef enum {STD, EXT} diag_data_t;    //!< diag data standard, nebo extended. Prevzato od Pepy.

/**
 * @brief Struktura zadosti o zaslani diagnostickych dat.
 *
 * Zadost o diagnosticka data vysilana DVRkem do IPU.
*/

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

typedef struct
{
    /** @brief typ paketu */
    uint8_t packetType;               /*[000]*/
    /** @brief pozadovany format odpovedi */
    uint8_t responseDataFormat;       /*[001]*/
    uint8_t reserved_00;              /*[002]*/
    uint8_t reserved_01;              /*[003]*/
    uint8_t reserved_02;              /*[004]*/
    uint8_t reserved_03;              /*[005]*/
    struct
    {
        /** @brief funkce DVR-04 @ VID1, 1 = ON / 0 = OFF */
        uint8_t enable_DVR_VID1 : 1; /*[0]*/
        /** @brief funkce DVR-04 @ VID2, 1 = ON / 0 = OFF */
        uint8_t enable_DVR_VID2 : 1; /*[1]*/
        uint8_t NOTUSED2 : 1; /*[2]*/
        uint8_t NOTUSED3 : 1; /*[3]*/
        uint8_t NOTUSED4 : 1; /*[4]*/
        uint8_t NOTUSED5 : 1; /*[5]*/
        uint8_t NOTUSED6 : 1; /*[6]*/
        /** @brief rizeni streamovani videa, 1 = ON / 0 = OFF */
        uint8_t enableStreaming : 1; /*[7]*/
    } videoStreamingControl;		 /*[006]*/
    struct
    {
        /** @brief 33 bit casove znacky */
        uint8_t timeStamp_33 : 1; /*[0]*/
        uint8_t NOTUSED1 : 1; /*[1]*/
        uint8_t NOTUSED2 : 1; /*[2]*/
        uint8_t NOTUSED3 : 1; /*[3]*/
        uint8_t NOTUSED4 : 1; /*[4]*/
        uint8_t NOTUSED5 : 1; /*[5]*/
        uint8_t NOTUSED6 : 1; /*[6]*/
        /** @brief 1 = platna casova znacka */
        uint8_t validTimeStamp : 1; /*[7]*/
    } timeStampControl;				  /*[007]*/
    /** @brief 32-bitova casova znacka */
    uint32_t timeStamp; 			  /*[008]*/
} sys_diag_request_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

/**
 * @brief Struktura standartni odpovedi na zadost o diagnosticka data.
 *
 * Diagnosticka data vyslana z IPU do DVR na zaklade zadosti z DVR, kratka verze, prakticky se nepoziva.
*/

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

typedef struct
{
    /** @brief typ paketu */
    uint8_t packetType;             //[000]
    /** @brief pocitadlo vyslanych paketu */
    uint8_t packetCounter;          //[001]
    /** @brief stavovy registr hwStatus */
    uint32_t hwStatus;              //[002]
    /** @brief stavovy registr swStatus */
    uint32_t swStatus;              //[006]
    char reserved_00[2];            //[010]
    /** @brief verze paketu */
    uint16_t packetVersion;         //[012]
    char reserved_01[3];            //[014]
    /** @brief viz SYSTEM_ID */
    uint8_t systemID;               //[017]
    char reserved_02[2];            //[018]
    /** @brief teplota CPU [°C x 1000], pozor, je to int */
    uint32_t temperatureCPU;        //[020]
    char reserved_03[4];            //[024]
    /** @brief CRC-32 aplikace ipu */
    uint32_t imageCRC;              //[028]
} SYS_DIAG_RESPONSE_STD_DATA_FORMAT;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#endif // DIAG_IPU_159__1_H
