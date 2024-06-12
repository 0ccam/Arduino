 /* Режимы работы. Нужное раскомментировать. */
//#define WASHMODE    // ПРОМЫВКА
#define DIVEMODE    // РОЗЛИВ С ПОГРУЖЕНИЕМ
//#define NODIVEMODE  // РОЗЛИВ БЕЗ ПОГРУЖЕНИЯ

#define Hstrt_mm    256   // Фактическая высота вилки от стола в мм при запуске станка.
#define Hmax_mm     320   // Максимально-возможная высота подъёма вилки.
#define Hmin_mm     25    // Минимально-возможная высота опускания вилки.
#define Tstep_max   255   // Максимально-возможная длительность шага вилки ШД.
#define Tstep_min   15    // Минимально-возможная длительность шага вилки ШД.
#define Vimp        9.4     // Количество мл для одной бутылки в одном импульсе от датчика насоса.
#define Kcor        0.0   // Коэффициент коррекции длительности шага ШД.

/* Соотношения величин. */
#define Htop_mm     (int)     (Hneck_mm - Hdpth_mm) // Высота верхней точки подъёма вилки, мм.
#define Nfull_imp   (float)   (Vfull_ml / Vimp)   // Число импульсов ДПР для налива полного объёма.
#define Npllw_imp   (float)   (Vpllw_ml / Vimp)   // Число импульсов ДПР для налива объёма подушки.
#define dV          (float)   (Vfull_ml - Vpllw_ml) // Полный объём налива бутылки за вычетом объёма подушки.
#define dH          (float)   (Hfull_mm - Hbttm_mm) // Высота границы полного налива за вычетом нижнего уровня вилки. 
#define Kflw        (float)   ((dV * 1.25) / (dH * Vimp)) // Коэффициент следования длительности шага за длительностью импульса ДПР.
