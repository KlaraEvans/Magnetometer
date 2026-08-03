#include "hardware/adc.h"
#include "math.h"
#include <algorithm>
class functions {
    public:
        static constexpr conversion_factor = 3.3*1000/(1<<12);
    //adc Messmethoden
        static void adc_standard(double *u_buf, int n){
            for (int i = 0; i < n; i++) {
                u_buf[i] = (double)adc_read() * conversion_factor;
            }
        }
        static void adc_fast(double *u_buf, int n){
            adc_set_clkdiv(0);        
            adc_fifo_setup(true, false, 1, false, false);
            adc_fifo_drain();
            adc_run(true);
            for(int i = 0; i < n; i++) {
                while (adc_fifo_is_empty());
                u_buf[i] = (double)adc_fifo_get() * conversion_factor;
            }
            adc_run(false);
            adc_fifo_drain();  
        }
    //Auswertungsmethoden
        static double method0(int adcMethod, int n){
            double spannung[n];
            switch(adcMethod){
                case 0:
                    adc_standard(spannung, n);
                    break;
                case 1:
                    adc_fast(spannung, n);
                    break;
            }
            double offset = 0;
            for(int i = 0; i < n; i++){
                offset += spannung[i];
            }
            offset /= n;
            double A = 0, B = 0;
            for(int i = 0; i < n; i++){
                spannung[i] -= offset;
                double phase = (double)i/n * (4.0*std::numbers::pi);
                B += spannung[i]*sin(phase);
                A += spannung[i]*cos(phase);
            }
            return sqrt(A*A + B * B)/n;
        }
        //TODO:Sin Cos Tabellen
        static double method1(int adcMethod, int n){
            double spannung[n];
            switch(adcMethod){
                case 0:
                    adc_standard(spannung, n);
                    break;
                case 1:
                    adc_fast(spannung, n);
                    break;
            }
            std::sort(spannung, spannung + n);
            double max = 0, min = 0;
            for(int i = 0; i < 2*k; i++) { 
                min += spannung[i];
                max += spannung[n-1-i];
            }
            max /= (2*k); min /= (2*k);
            double U2 = (max - min)/2.0;
            return U2;
        }
        static double method2(int adcMethod, int n){
            double spannung[n];
            switch(adcMethod){
                case 0:
                    adc_standard(spannung, n);
                    break;
                case 1:
                    adc_fast(spannung, n);
                    break;
            }
            double sum_u = 0;
            for(int i = 0; i < n; i++) { 
                sum_u += spannung[i];
            }
            double off_u = sum_u / n;
            double sq_u = 0;
            for(int i = 0; i < n; i++) {
                double du = spannung[i] - off_u;
                sq_u += du * du;
            }
            return sqrt(sq_u / n) * sqrt(2);
        }
};