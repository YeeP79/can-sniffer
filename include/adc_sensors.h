#ifndef ADC_SENSORS_H
#define ADC_SENSORS_H

using AdcCallback = void(*)(float tv1, float tv2, float psi_f, float psi_r);

void adc_set_log_callback(AdcCallback cb);
void ads_init();
void ads_loop();

#endif // ADC_SENSORS_H
