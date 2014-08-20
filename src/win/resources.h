/* Menu Items */
#define IDM_FILE_RESET		40000
#define IDM_FILE_EXIT		40001
#define IDM_DISC_LD0		40010
#define IDM_DISC_LD1		40011
#define IDM_CONFIG		40052
#define IDM_NETWORKING		40066
#define IDM_FULLSCR		40054
#define IDM_CPUIDLE		40055
#define IDM_CDROM_DISABLED	40060
#define IDM_CDROM_EMPTY		40061
#define IDM_CDROM_ISO		40062
#define IDM_CDROM_REAL		40100
#define IDM_MOUSE_FOL		40064
#define IDM_MOUSE_CAP		40065
#define IDM_MOUSE_TWOBUTTON	40067
#define IDM_HELP_ONLINE_MANUAL	40080
#define IDM_HELP_VISIT_WEBSITE	40081
#define IDM_HELP_ABOUT		40082
/* Last ID */

/* Dialog IDs */
#define IDD_ABOUT	100

/* Indexes into the items of the CONFIGUREDLG Dialog */
#define ListBox_Hardware	1000

#define RadioButton_Mem_4	1006
#define RadioButton_Mem_8	1007
#define RadioButton_Mem_16	1008
#define RadioButton_Mem_32	1009
#define RadioButton_Mem_64	1010
#define RadioButton_Mem_128	1011
#define RadioButton_Mem_256	1012

#define RadioButton_VRAM_0	1013
#define RadioButton_VRAM_2	1014

#define GroupBox_CPU		1020
#define GroupBox_RAM		1021
#define GroupBox_VRAM		1022
#define GroupBox_Refresh	1023

#define CheckBox_Sound		1030

#define Slider_Refresh		1040

#define Text_Refresh		1050

/* Indexes into the items of the NETWORKDLG Dialog */
#define RadioButton_Off			2000
#define RadioButton_EthernetBridging	2001
#define Edit_BridgeName			2002
#define Text_BridgeName			2003
#define GroupBox_Networking		2004

/* Indexes into the items of the About Dialog */
#define Icon_App	3000
#define Text_Name	3001
#define Text_Version	3002
#define Text_Date	3003
#define Text_Licence	3004
