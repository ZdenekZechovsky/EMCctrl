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

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

/**
 * @brief Struktura rozsirene odpovedi na zadost o diagnosticka data.
 *
 * Diagnosticka data vyslana z IPU do DVR na zaklade zadosti z DVR, plna verze.
*/
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
    /** @brief nazev firmwaru vcetne verze, viz FIRMWARE_NAME */
    char firmwareName[24];          //[032]
    /** @brief citac oprav PTS */
    uint16_t corrPTScounter;        //[056]
    /** @brief citac odeslanych MPEG TS paketu */
    uint32_t TSpacketCounter;       //[058]

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Plne statusy jednotlivych vlaken. */
    struct
    {
        /** @brief status hlavniho vlakna (tzv. server), ozn. main thread */
        uint8_t servStat;       //[000]
        /** @brief status vlaknas GST pipeline, ozn. GST pipeline thread */
        uint8_t pipStat;        //[001]
        /** @brief status vlakna tzv. UDP clienta, ozn. diag. and PTS thread */
        uint8_t udpStat;        //[002]
        /** @brief status vlakna kalibrace obrazu, ozn. calib. thread */
        uint8_t kalibStat;      //[003]
    } swStats;	//[062]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

    /** @brief citac prijatych kalibracnich paketu */
    uint8_t kalibPackCounter;       //[066]

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Prijate rozliseni PDU, preposlano beze zmeny. */
    struct
    {
        /** @brief prijate rozliseni PDU v ose X */
        uint16_t PDUResolutionX;//[000]
        /** @brief prijate rozliseni PDU v ose Y */
        uint16_t PDUResolutionY;//[001]
    } PDUResolution;	//[067]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Rozliseni obrazu z kamery na vstupu kompositoru. */
    struct
    {
        /** @brief sirka obrazu upravena podle skalovaciho koeficientu */
        uint16_t CamResolutionX;//[000]
        /** @brief vyska obrazu upravena podle skalovaciho koeficientu */
        uint16_t CamResolutionY;//[002]
    } CamResolution;	//[071]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

    /** @brief citac re-enablovani streamu z tc358743 po vypadku a obnoveni signalu SDI */
    uint16_t TCStreamEnableCounter; //[075]

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Offsety, posunuti obrazu z kalibrace PDU, v pixelech. */
    struct
    {
        /** @brief offset v ose X, + doprava */
        int16_t offsetX;       //[000]
        /** @brief offset v ose Y, + nahoru */
        int16_t offsetY;       //[002]
    } Offsets;	//[077]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Obsah vybranych registru ds90ub954, obsah struktury zkopirovan z ds90ub954.h !!!. */
    struct
    {
        /** @brief addr. 0x04, puntiky */
        uint8_t device_sts;     //[000]
        /** @brief addr. 0x35, puntiky */
        uint8_t csi_sts;        //[001]
        /** @brief addr. 0x4D, puntiky */
        uint8_t rx_port1_sts;   //[002]
        /** @brief addr. 0x4E, puntiky */
        uint8_t rx_port2_sts;   //[003]
        /** @brief addr. 0x55, puntiky */
        uint8_t rx_par_err_hi;  //[004]
        /** @brief addr. 0x56, puntiky */
        uint8_t rx_par_err_lo;  //[005]
        /** @brief addr. 0x73, cislo */
        uint8_t line_count_hi;  //[006]
        /** @brief addr. 0x74, cislo */
        uint8_t line_count_lo;  //[007]
        /** @brief addr. 0x75, cislo */
        uint8_t line_len1;      //[008]
        /** @brief addr. 0x76, cislo */
        uint8_t line_len0;      //[009]
        /** @brief addr. 0x7A, puntiky */
        uint8_t csi_rx_sts;     //[010]
        /** @brief addr. 0x7B, cislo */
        uint8_t csi_err_counter;//[011]
        /** @brief chyby pri cteni, 0 je ok, pridano do struktury z ds90ub954.h, vysledek fce DS90UB954_get_diag(), puntiky */
        uint16_t i2c_read_sts;	//[012]
    } Diag_ds90ub954;	//[081]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Obsah vybranych registru ds90ub953, , obsah struktury zkopirovan z ds90ub954.h !!!. */
    struct
    {
        /** @brief addr. 0x52, puntiky */
        uint8_t general_status;     //[000]
        /** @brief addr. 0x55, cislo */
        uint8_t crc_err_cnt1;       //[001]
        /** @brief addr. 0x56, cislo */
        uint8_t crc_err_cnt2;       //[002]
        /** @brief addr. 0x5C, cislo */
        uint8_t csi_err_cnt;        //[003]
        /** @brief addr. 0x5D, puntiky */
        uint8_t csi_err_status;     //[004]
        /** @brief addr. 0x5E, puntiky */
        uint8_t csi_err_dlane01;    //[005]
        /** @brief addr. 0x5F, puntiky */
        uint8_t csi_err_dlane23;    //[006]
        /** @brief addr. 0x60, puntiky */
        uint8_t csi_err_clk_lane;   //[007]
        /** @brief addr. 0x64, puntiky */
        uint8_t csi_ecc;            //[008]
        /** @brief chyby pri cteni, 0 je ok, pridano do struktury z ds90ub954.h, vysledek fce DS90UB953_get_diag(), puntiky */
        uint16_t i2c_read_sts;		//[009]
    } Diag_ds90ub953;	//[095]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief vystupni bitrate v kb/s, popis: bitrate [kb/s] */
    uint32_t bitrate;               //[106]

    /** @brief Diagnostika hw casti od ZZ. */
    struct
    {
        /** @brief Message Counter, cislo dec */
        uint16_t messageCounter;					//[000]
        /** @brief Counter of Received Valid CPU's Messages, cislo dec */
        uint32_t rcvValidMsgCounter;				//[002]
        /** @brief firmware CRC-32 stored in internal flash, cislo hex */
        uint32_t firmwareCRC;						//[006]
        /** @brief Firmware ID - BCD x 10, i.e. 0x0010 ~ 1.0, cislo dec x.y */
        uint16_t firmwareID;						//[010]
        /** @brief on board temperature sensor, signed, t[°C] = temperature / 256.0, cislo ve °C */
        int16_t temperature;						//[012]
        /** @brief diagnostic status, puntiky */
        uint16_t diagnosticStatus;				    //[014]
        /** @brief GS12170 subaddress 0x0003, cislo hex */
        uint16_t GS12170_input_lock;				//[016]
        /** @brief GS12170 subaddress 0x0007, cislo hex */
        uint16_t GS12170_data_rate_report;			//[018]
        /** @brief GS12170 subaddress 0x00BD, cislo hex */
        uint16_t GS12170_SDI_video_error_status;	//[020]
        /** @brief GS12170 subaddress 0x00BE, cislo hex */
        uint16_t GS12170_SDI_video_status;			//[022]
        /** @brief GS12170 subaddress 0x00BF, cislo hex */
        uint16_t GS12170_raster_struc_1;			//[024]
        /** @brief GS12170 subaddress 0x00C0, cislo hex */
        uint16_t GS12170_raster_struc_2;			//[026]
        /** @brief GS12170 subaddress 0x00C1, cislo hex */
        uint16_t GS12170_raster_struc_3;			//[028]
        /** @brief GS12170 subaddress 0x00C2, cislo hex */
        uint16_t GS12170_raster_struc_4;			//[030]
        /** @brief status cteni  SPI linky, 0 = ok, puntiky */
        uint8_t uart_read_sts;						//[032]
    } Diag_ZZ;	//[110]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

    /** @brief Data z kamery. */
    struct
    {
        /** @brief citlivost z IF_AE_G_ISO, viz IMX8MPCDUG */
        uint16_t iso;                               //[000]
        /** @brief gain z IF_EC_G_CFG, viz IMX8MPCDUG, float x 1000 */
        uint16_t gain;                              //[002]
        /** @brief expozicni doba z IF_EC_G_CFG, viz IMX8MPCDUG, [us] */
        uint16_t expTime;                           //[004]
        /** @brief prumerny jas z IF_AE_G_STATUS, viz IMX8MPCDUG */
        uint8_t averageLuminance;                   //[006]
    } Image_Sensor_Data;	//[143]

#ifdef _MSC_VER
#pragma pack(pop)
#endif

    /** @brief  GS12170 subaddress 0x008C, cislo hex (soucast hw diag. od ZZ, dodatecne doplneno kvuli zpet. kompat. na konec)*/
    uint16_t GS12170_CRC_error_CH0_counter;     //[150]

    /** @brief DVR video enable a stream enable*/
    uint8_t DVR_video_enable;                   //[152]

    char reserved_04[103];          /* [153] rezervovano */
}SYS_DIAG_RESPONSE_EXT_DATA_FORMAT;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#endif // DIAG_IPU_159__1_H
