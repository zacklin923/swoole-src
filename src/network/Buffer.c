/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#include "swoole.h"
#include "Server.h"

/**
 * create new buffer
 */
swBuffer* swBuffer_new(int trunk_size)
{
	swBuffer *buffer = sw_malloc(sizeof(swBuffer));
	if (buffer == NULL)
	{
		swWarn("malloc for buffer failed. Error: %s[%d]", strerror(errno), errno);
		return NULL;
	}

	bzero(buffer, sizeof(swBuffer));
	buffer->trunk_size = trunk_size;

	return buffer;
}

/**
 * create new trunk
 */
swBuffer_trunk *swBuffer_new_trunk(swBuffer *buffer, uint32_t type, uint16_t size)
{
	swBuffer_trunk *trunk = sw_malloc(sizeof(swBuffer_trunk));
	if (trunk == NULL)
	{
		swWarn("malloc for trunk failed. Error: %s[%d]", strerror(errno), errno);
		return NULL;
	}

	bzero(trunk, sizeof(swBuffer_trunk));

	//require alloc memory
	if (type == SW_TRUNK_DATA && size > 0)
	{
		void *buf = sw_malloc(size);
		if (buf == NULL)
		{
			swWarn("malloc for data failed. Error: %s[%d]", strerror(errno), errno);
			sw_free(trunk);
			return NULL;
		}
		trunk->data = buf;
	}

	trunk->type = type;
	buffer->trunk_num ++;

	if (buffer->head == NULL)
	{
		buffer->tail = buffer->head = trunk;
	}
	else
	{
		buffer->tail->next = trunk;
		buffer->tail = trunk;
	}
	return trunk;
}

/**
 * pop the head trunk
 */
SWINLINE void swBuffer_pop_trunk(swBuffer *buffer, swBuffer_trunk *trunk)
{
	//only one trunk
	if (trunk->next == NULL)
	{
		buffer->head = NULL;
		buffer->tail = NULL;
		buffer->trunk_num = 0;
	}
	else
	{
		buffer->head = trunk->next;
		buffer->trunk_num --;
	}

	if (trunk->type == SW_TRUNK_DATA)
	{
		sw_free(trunk->data);
	}
	sw_free(trunk);
}

/**
 * free buffer
 */
int swBuffer_free(swBuffer *buffer)
{
	swBuffer_trunk *trunk = buffer->head;
	swBuffer_trunk *will_free_trunk; //free the point
	while (trunk != NULL)
	{
		if (trunk->type == SW_TRUNK_DATA)
		{
			sw_free(trunk->data);
		}
		will_free_trunk = trunk;
		trunk = trunk->next;
		sw_free(will_free_trunk);
	}
	sw_free(buffer);
	return SW_OK;
}

/**
 * append to buffer queue
 */
int swBuffer_append(swBuffer *buffer, void *data, uint32_t size)
{
	swBuffer_trunk *trunk = swBuffer_new_trunk(buffer, SW_TRUNK_DATA, size);
	if (trunk == NULL)
	{
		return SW_ERR;
	}

	buffer->length += size;
	trunk->length = size;

	memcpy(trunk->data, data, trunk->length);

	swTraceLog(SW_TRACE_BUFFER, "trunk_n=%d|size=%d|trunk_len=%d|trunk=%p", buffer->trunk_num, size,
			trunk->length, trunk);

	return SW_OK;
}

/**
 * send buffer to client
 */
int swBuffer_send(swBuffer *buffer, int fd)
{
	int ret, sendn;
	swBuffer_trunk *trunk = swBuffer_get_trunk(buffer);
	sendn = trunk->length - trunk->offset;

	if (sendn == 0)
	{
		swBuffer_pop_trunk(buffer, trunk);
		return SW_CONTINUE;
	}
	ret = send(fd, trunk->data + trunk->offset, sendn, 0);
	//printf("BufferOut: reactor=%d|sendn=%d|ret=%d|trunk->offset=%d|trunk_len=%d\n", reactor->id, sendn, ret, trunk->offset, trunk->length);
	if (ret < 0)
	{
		if (swConnection_error(fd, errno) < 0)
		{
			return SW_CLOSE;
		}
		else if(errno == EAGAIN)
		{
			return SW_WAIT;
		}
		else
		{
			swWarn("send to fd[%d] failed. Error: %s[%d]", fd, strerror(errno), errno);
			return SW_CONTINUE;
		}
	}
	//trunk full send
	else if(ret == sendn || sendn == 0)
	{
		swBuffer_pop_trunk(buffer, trunk);
	}
	else
	{
		trunk->offset += ret;
	}
	return SW_CONTINUE;
}

/**
 * print buffer
 */
void swBuffer_debug(swBuffer *buffer)
{
	int i = 0;
	swBuffer_trunk *trunk = buffer->head;
	printf("%s\n%s\n", SW_START_LINE, __func__);
	while (trunk != NULL && trunk->next != NULL)
	{
		i++;
		printf("%d.\tlen=%d\tdata=%s\n", i, trunk->length, (char *)trunk->data);
		trunk = trunk->next;
	}
	printf("%s\n%s\n", SW_END_LINE, __func__);
}