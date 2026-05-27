#ifndef GD32F103_H
#define GD32F103_H

#include <stdint.h>

// =============================================
// GD32F103C8T6 Base addresses
// =============================================
#define PERIPH_BASE         0x40000000UL
#define APB1PERIPH_BASE     (PERIPH_BASE)
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x10000UL)
#define AHBPERIPH_BASE      (PERIPH_BASE + 0x20000UL)

// RCC
#define RCC_BASE            (AHBPERIPH_BASE + 0x1000UL)
typedef struct {
    volatile uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR;
    volatile uint32_t AHBENR, APB2ENR, APB1ENR;
    volatile uint32_t BDCR, CSR;
} RCC_TypeDef;
#define RCC                 ((RCC_TypeDef *)RCC_BASE)

// GPIO
#define GPIOA_BASE          (APB2PERIPH_BASE + 0x0800UL)
#define GPIOB_BASE          (APB2PERIPH_BASE + 0x0C00UL)
#define GPIOC_BASE          (APB2PERIPH_BASE + 0x1000UL)
typedef struct {
    volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
} GPIO_TypeDef;
#define GPIOA               ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC               ((GPIO_TypeDef *)GPIOC_BASE)

// TIM1 (Advanced - PWM for motor)
#define TIM1_BASE           (APB2PERIPH_BASE + 0x2C00UL)
typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR;
    volatile uint32_t CCMR1, CCMR2, CCER, CNT, PSC, ARR;
    volatile uint32_t RCR, CCR1, CCR2, CCR3, CCR4;
    volatile uint32_t BDTR, DCR, DMAR;
} TIM_TypeDef;
#define TIM1                ((TIM_TypeDef *)TIM1_BASE)

// TIM2 (General - ADC trigger / Hall decode)
#define TIM2_BASE           (APB1PERIPH_BASE + 0x0000UL)
#define TIM2                ((TIM_TypeDef *)TIM2_BASE)

// ADC1
#define ADC1_BASE           (APB2PERIPH_BASE + 0x2400UL)
typedef struct {
    volatile uint32_t SR, CR1, CR2, SMPR1, SMPR2;
    volatile uint32_t JOFR1, JOFR2, JOFR3, JOFR4;
    volatile uint32_t HTR, LTR, SQR1, SQR2, SQR3;
    volatile uint32_t JSQR, JDR1, JDR2, JDR3, JDR4, DR;
} ADC_TypeDef;
#define ADC1                ((ADC_TypeDef *)ADC1_BASE)

// USART1
#define USART1_BASE         (APB2PERIPH_BASE + 0x3800UL)
typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_TypeDef;
#define USART1              ((USART_TypeDef *)USART1_BASE)

// AFIO
#define AFIO_BASE           (APB2PERIPH_BASE + 0x0000UL)
typedef struct {
    volatile uint32_t EVCR, MAPR, EXTICR[4], RESERVED, MAPR2;
} AFIO_TypeDef;
#define AFIO                ((AFIO_TypeDef *)AFIO_BASE)

// SysTick
#define SysTick_BASE        0xE000E010UL
typedef struct {
    volatile uint32_t CTRL, LOAD, VAL, CALIB;
} SysTick_TypeDef;
#define SysTick             ((SysTick_TypeDef *)SysTick_BASE)

// NVIC
#define NVIC_BASE           0xE000E100UL
typedef struct {
    volatile uint32_t ISER[8], RESERVED0[24];
    volatile uint32_t ICER[8], RESERVED1[24];
    volatile uint32_t ISPR[8], RESERVED2[24];
    volatile uint32_t ICPR[8], RESERVED3[24];
    volatile uint32_t IABR[8], RESERVED4[56];
    volatile uint8_t  IP[240];
    volatile uint32_t RESERVED5[644];
    volatile uint32_t STIR;
} NVIC_TypeDef;
#define NVIC                ((NVIC_TypeDef *)NVIC_BASE)

// RCC bits
#define RCC_APB2ENR_IOPAEN  (1U << 2)
#define RCC_APB2ENR_IOPBEN  (1U << 3)
#define RCC_APB2ENR_IOPCEN  (1U << 4)
#define RCC_APB2ENR_ADC1EN  (1U << 9)
#define RCC_APB2ENR_TIM1EN  (1U << 11)
#define RCC_APB2ENR_USART1EN (1U << 14)
#define RCC_APB2ENR_AFIOEN  (1U << 0)
#define RCC_APB1ENR_TIM2EN  (1U << 0)
#define RCC_CR_HSEON        (1U << 16)
#define RCC_CR_HSERDY       (1U << 17)
#define RCC_CR_PLLON        (1U << 24)
#define RCC_CR_PLLRDY       (1U << 25)
#define RCC_CFGR_PLLSRC     (1U << 16)
#define RCC_CFGR_PLLMULL9   (0x7U << 18)
#define RCC_CFGR_PPRE1_DIV2 (0x4U << 8)
#define RCC_CFGR_SW_PLL     (0x2U << 0)
#define RCC_CFGR_SWS_PLL    (0x2U << 2)

// System clock = 72MHz (8MHz HSE * 9)
#define SYSCLK_HZ           72000000UL
#define APB1_HZ             36000000UL
#define APB2_HZ             72000000UL

// Utility macros
#define SET_BIT(reg, bit)   ((reg) |= (bit))
#define CLR_BIT(reg, bit)   ((reg) &= ~(bit))
#define READ_BIT(reg, bit)  ((reg) & (bit))

static inline void nvic_enable_irq(uint8_t irq) {
    NVIC->ISER[irq >> 5] = (1U << (irq & 0x1F));
}

#endif // GD32F103_H
