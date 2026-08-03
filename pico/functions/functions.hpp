#pragma once
class functions {
    public:
    //Konstanten
        static double pi;
        static double conversion_factor;
    //adc Messmethoden
        static void adc_standard(double *u_buf, int n);
        static void adc_fast(double *u_buf, int n);
    //Auswertungsmethoden
        static double method0(int adcMethod, int n);
        //TODO:TABLES
        static double method1(int adcMethod, int n);
        static double method2(int adcMethod, int n);
};