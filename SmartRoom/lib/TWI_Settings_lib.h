#ifndef TWI_SETTINGS_LIB_H
#define TWI_SETTINGS_LIB_H
/*
||
||  Filename:	 		TWI_Settings.h
||  Title: 			    TWI Driver Settings
||  Author: 			Efthymios Koktsidis
||	Email:				efthymios.ks@gmail.com
||  Compiler:		 	AVR-GCC
||	Description:
||	Settings for the TWI hardware.
||
*/

//----- Configuration -------------//
//SCL Frequency
#define F_SCL				400000UL
#define F_CPU				8000000UL

//TWI pins
#define TWI_SCL				C, 0
#define TWI_SDA				C, 1
//---------------------------------//
#endif