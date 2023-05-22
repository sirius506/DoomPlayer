#ifndef __HCI_IF_H__
#define __HCI_IF_H__

#define	HCIEVQ_DEPTH	4
#define	BUFFQ_DEPTH	2

typedef struct {
  uint8_t packet_type;
  uint8_t *packet;
  uint16_t size;
} HCIEVT;

#define	DMA_EVINPUT	(1<<0)
#define	DMA_ACLINPUT	(1<<1)
#define	DMA_ACLOUTPUT	(1<<2)

typedef struct {
  osMessageQueueId_t hcievqId;
  osMessageQueueId_t evbuffqId;
  osMessageQueueId_t aclbuffqId;
  osEventFlagsId_t   dongle_flagId;
  uint8_t    *ev_buffer;	/* active event buffer */
  uint16_t   evb_offset;	/* Event buffer offset */
  uint8_t    *acl_buffer;	/* Active ACL buffer */
  uint16_t   acl_offset;	/* ACL input offset */
  uint16_t   acl_out_offset;	/* ACL output offset */
  uint8_t    dma_flag;
} HCIIF_INFO;

extern HCIIF_INFO HciIfInfo;

#endif
