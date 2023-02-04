/**
 * Console I/O Task 
 */
#include "DoomPlayer.h"
#include <stdlib.h>

#define	TXBSIZE	300

extern UART_HandleTypeDef huart1;

static SECTION_AHBSRAM char tx_ring_buffer[TXBSIZE];

typedef struct {
  char *btop;	/* Ring buffer start address */
  char *pput;	/* Put pointer */
  char *pget;	/* Get pointer */
  int  count;
  int  txcount;
  osSemaphoreId_t roomsem;
  osMutexId_t     lock;
} RING_DESC;

static SECTION_AHBSRAM RING_DESC tx_desc;

typedef struct {
  osMessageQueueId_t evq;
  RING_DESC         *txring;
} CONSTASKINFO;

SECTION_AHBSRAM CONSTASKINFO ConsTaskInfo;

typedef enum {
  EV_TXREQ = 1,
  EV_TXDONE,
  EV_RXDONE,
} EVCODE;

SEMAPHORE_DEF(utxsem)

osStaticMutexDef_t utxlockControlBlock;

const osMutexAttr_t utxlock_attributes = {
    .name = "utxlock",
    .cb_mem = &utxlockControlBlock,
    .cb_size = sizeof(utxlockControlBlock),
};

#define	LOG_START_MES	"Log output started.\r\n"

#define	CONSQ_DEPTH	4

SECTION_DTCMRAM static uint8_t consqBuffer[ CONSQ_DEPTH * sizeof( uint16_t ) ];

MESSAGEQ_DEF(consq, consqBuffer, sizeof(consqBuffer))


#define	DEVQ_DEPTH	6

int _write(int file, char *ptr, int len)
{
  int ns;
  CONSTASKINFO *consInfo = &ConsTaskInfo;
  RING_DESC *ring;
  int cpush;
  uint16_t evcode = EV_TXREQ;

  ns = len;

  ring = consInfo->txring;
  cpush = 0;

  while (len > 0)
  {
    osSemaphoreAcquire(ring->roomsem, osWaitForever);

    osMutexAcquire(ring->lock, osWaitForever);
    ring->pput = ring->btop;

    do
    {
      if (cpush == 0)
      {
        if (*ptr == '\n')
        {
          cpush = '\r';
          *ring->pput++ = cpush;
        }
        else
        {
          *ring->pput++ = *ptr++;
          len--;
        }
      }
      else
      {
        cpush = 0;
        *ring->pput++ = *ptr++;
        len--;
      }
      ring->count++;
    } while (len > 0 && ring->count < TXBSIZE);
    osMutexRelease(ring->lock);
    osMessageQueuePut(consInfo->evq, &evcode, 0, 0);
  }
  return ns;
}

static void txCallback()
{
  CONSTASKINFO *consInfo = &ConsTaskInfo;
  uint16_t evcode;

  evcode = EV_TXDONE;
  osMessageQueuePut(consInfo->evq, &evcode, 0, 0);
}

#if 0
static void rxCallback()
{
  CONSTASKINFO *consInfo = &ConsTaskInfo;
  uint16_t evcode;

  evcode = EV_RXDONE;
  osMessageQueuePut(consInfo->evq, &evcode, 0, 0);
}
#endif

void StartConsoleTask(void *argument)
{
  uint16_t evcode;
  CONSTASKINFO *consInfo = &ConsTaskInfo;
  RING_DESC *ring;
  char consByte;
#if 0
  int outc;
#endif
  UART_HandleTypeDef *huart = &huart1;

  consInfo->txring = &tx_desc;
  consInfo->txring->btop = tx_ring_buffer;
  consInfo->txring->pput = tx_ring_buffer;
  consInfo->txring->pget = tx_ring_buffer;
  consInfo->txring->count = 0;
  consInfo->txring->txcount = 0;
  consInfo->txring->roomsem = osSemaphoreNew(1, 1, &attributes_utxsem);
  consInfo->txring->lock = osMutexNew(&utxlock_attributes);;
  
  consInfo->evq = osMessageQueueNew (CONSQ_DEPTH, sizeof(uint16_t), &attributes_consq);

  HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID, txCallback);
#if 0
  HAL_UART_RegisterCallback(huart, HAL_UART_RX_COMPLETE_CB_ID, rxCallback);
#endif

  HAL_UART_Receive_IT(huart, (uint8_t *)&consByte, 1);

  ring = consInfo->txring;
  ring->count = strlen(LOG_START_MES);
  memcpy(ring->btop, LOG_START_MES, ring->count);
  SCB_CleanDCache_by_Addr((uint32_t *)ring->btop, ring->count);
#if 0
  HAL_UART_Transmit_IT(huart, (uint8_t *)ring->btop, ring->count);
#else
  HAL_UART_Transmit_DMA(huart, (uint8_t *)ring->btop, ring->count);
#endif
  ring->txcount = ring->count;

  while (1)
  {
    osMessageQueueGet(consInfo->evq, &evcode, NULL, osWaitForever);

    switch (evcode)
    {
    case EV_TXREQ:
      ring = consInfo->txring;
      if ((ring->count > 0) && (ring->txcount == 0))
      {
        SCB_CleanDCache_by_Addr((uint32_t *)ring->btop, ring->count);
#if 0
        HAL_UART_Transmit_IT(huart, (uint8_t *)ring->btop, ring->count);
#else
        HAL_UART_Transmit_DMA(huart, (uint8_t *)ring->btop, ring->count);
#endif
        ring->txcount = ring->count;
      }
      break;
    case EV_TXDONE:
      ring = consInfo->txring;
      if (ring->txcount)
      {
        osMutexAcquire(ring->lock, osWaitForever);
        ring->count = 0;
        ring->txcount = 0;
        osMutexRelease(ring->lock);
        osSemaphoreRelease(ring->roomsem);
      }
      break;
#if 0
    case EV_RXDONE:
      outc = consByte;
      if (outc)
      {
        app_send_keycode(outc);
      }
      HAL_UART_Receive_IT(huart, (uint8_t *)&consByte, 1);
      break;
#endif
    default:
      break;
    }
  }
}
