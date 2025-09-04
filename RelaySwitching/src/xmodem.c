/*
 * xmodem.cpp
 *
 *  Created on: Mar 11, 2025
 *      Author: user
 */

#include "xmodem.h"

extern uint16_t DataBufferTotalSize ;
extern void ledfasttoggle(uint8_t toggling);
/* Global variables. */
static uint8_t xmodem_packet_number = 1u;       /**< Packet number counter. */
static uint8_t x_first_packet_received = false; /**< First packet or not. */

/* Local functions. */

static uint16_t xmodem_calc_crc(uint8_t *data, uint16_t length);

static xmodem_status xmodem_handle_packet(uint8_t *buffer, uint8_t size, uint8_t (*rx)(uint8_t *, uint16_t, uint32_t), uint8_t (*tx)(uint8_t, uint32_t));

static xmodem_status xmodem_error_handler(uint8_t *error_number, uint8_t max_error_number, uint8_t (*rx)(uint8_t *, uint16_t, uint32_t), uint8_t (*tx)(uint8_t, uint32_t));

static uint32_t total_size = 0;
const uint32_t PROTOCOL_TIMEOUT = 1500;
const uint8_t protocol_ok = 0;

/**
 * @brief   This function is the base of the Xmodem protocol.
 *          When we receive a header from UART, it decides what action it shall take.
 * @param   void
 * @return  void
 */

uint8_t xmodem_receive(uint8_t *buffer, uint8_t (*rx)(uint8_t *, uint16_t, uint32_t), uint8_t (*tx)(uint8_t, uint32_t))
{
    volatile xmodem_status status = X_OK;
    uint8_t error_number = 0u;

    x_first_packet_received = false;
    xmodem_packet_number = 1u;
   
    /* Loop until there isn't any error (or until we jump to the user application). */
    while (X_OK == status)
    {
        //	  HAL_IWDG_Refresh(&hiwdg);
        uint8_t header = 0x00u;

        /* Get the header from UART. */
        uint8_t comm_status = rx(&header, 1u, PROTOCOL_TIMEOUT);

        /* Spam the host (until we receive something) with ACSII "C", to notify it, we want to use CRC-16. */
        if ((protocol_ok != comm_status) && (false == x_first_packet_received))
        {
            tx(X_C, PROTOCOL_TIMEOUT);
        }
        /* Uart timeout or any other errors. */
        else if ((protocol_ok != comm_status) && (true == x_first_packet_received))
        {
            status = xmodem_error_handler(&error_number, X_MAX_ERRORS, rx, tx);
        }
        else
        {
            /* Do nothing. */
        }
        xmodem_status packet_status = X_ERROR;
        /* The header can be: SOH, STX, EOT and CAN. */
        switch (header)
        {

        /* 128 or 1024 bytes of data. */
        case X_SOH:
        case X_STX:
            /* If the handling was successful, then send an ACK. */
            packet_status = xmodem_handle_packet(buffer, header, rx, tx);
            if (X_OK == packet_status)
            {
                (void)tx(X_ACK, PROTOCOL_TIMEOUT);
            }
            /* If the error was flash related, then immediately set the error counter to max (graceful abort). */
            else if (X_ERROR_FLASH == packet_status)
            {
                error_number = X_MAX_ERRORS;
                status = xmodem_error_handler(&error_number, X_MAX_ERRORS, rx, tx);
            }
            /* Error while processing the packet, either send a NAK or do graceful abort. */
            else
            {
                status = xmodem_error_handler(&error_number, X_MAX_ERRORS, rx, tx);
            }
            break;
        /* End of Transmission. */
        case X_EOT:

            /* ACK, feedback to user (as a text) */
            (void)tx(X_ACK, PROTOCOL_TIMEOUT);

            k_msleep(500);
            ledfasttoggle(20);
             total_size = 0;
            return;
            break;
        /* Abort from host. */
        case X_CAN:
            status = X_ERROR;
            break;
        default:
            /* Wrong header. */
            if (protocol_ok == comm_status)
            {
                status = xmodem_error_handler(&error_number, X_MAX_ERRORS, rx, tx);
            }
            break;
        }
    }
}

/**
 * @brief   Calculates the CRC-16 for the input package.
 * @param   *data:  Array of the data which we want to calculate.
 * @param   length: Size of the data, either 128 or 1024 bytes.
 * @return  status: The calculated CRC.
 */
static uint16_t xmodem_calc_crc(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0u;
    while (length)
    {
        length--;
        crc = crc ^ ((uint16_t)*data++ << 8u);
        for (uint8_t i = 0u; i < 8u; i++)
        {
            if (crc & 0x8000u)
            {
                crc = (crc << 1u) ^ 0x1021u;
            }
            else
            {
                crc = crc << 1u;
            }
        }
    }
    return crc;
}

/**
 * @brief   This function handles the data packet we get from the xmodem protocol.
 * @param   header: SOH or STX.
 * @return  status: Report about the packet.
 */

static xmodem_status xmodem_handle_packet(uint8_t *buffer, uint8_t header, uint8_t (*rx)(uint8_t *, uint16_t, uint32_t), uint8_t (*tx)(uint8_t, uint32_t))
{
    xmodem_status status = X_OK;
    uint16_t size = 0u;

    /* 2 bytes for packet number, 1024 for data, 2 for CRC*/
    uint8_t received_packet_number[X_PACKET_NUMBER_SIZE];
    uint8_t received_packet_data[X_PACKET_1024_SIZE];
    uint8_t received_packet_crc[X_PACKET_CRC_SIZE];

    /* Get the size of the data. */
    if (X_SOH == header)
    {
        size = X_PACKET_128_SIZE;
    }
    else if (X_STX == header)
    {
        size = X_PACKET_1024_SIZE;
    }
    else
    {
        /* Wrong header type. This shoudn't be possible... */
        status |= X_ERROR;
    }

    uint8_t comm_status = protocol_ok;
    /* Get the packet number, data and CRC from UART. */
    comm_status |= rx(&received_packet_number[0u], X_PACKET_NUMBER_SIZE, PROTOCOL_TIMEOUT);
    comm_status |= rx(&received_packet_data[0u], size, PROTOCOL_TIMEOUT);
    comm_status |= rx(&received_packet_crc[0u], X_PACKET_CRC_SIZE, PROTOCOL_TIMEOUT);
    /* Merge the two bytes of CRC. */
    uint16_t crc_received = ((uint16_t)received_packet_crc[X_PACKET_CRC_HIGH_INDEX] << 8u) | ((uint16_t)received_packet_crc[X_PACKET_CRC_LOW_INDEX]);
    /* We calculate it too. */
    uint16_t crc_calculated = xmodem_calc_crc(&received_packet_data[0u], size);

    /* Communication error. */
    if (protocol_ok != comm_status)
    {

        status |= X_ERROR_UART;
    }

    /* If it is the first packet, then erase the memory. */
    if ((X_OK == status) && (false == x_first_packet_received))
    {
    }

    /* Error handling and flashing. */
    if (X_OK == status)
    {
        if (xmodem_packet_number != received_packet_number[0u])
        {
            /* Packet number counter mismatch. */
            status |= X_ERROR_NUMBER;
        }
        if (255u != (received_packet_number[X_PACKET_NUMBER_INDEX] + received_packet_number[X_PACKET_NUMBER_COMPLEMENT_INDEX]))
        {
            /* The sum of the packet number and packet number complement aren't 255. */
            /* The sum always has to be 255. */

            status |= X_ERROR_NUMBER;
        }
        if (crc_calculated != crc_received)
        {
            /* The calculated and received CRC are different. */

            status |= X_ERROR_CRC;
        }
    }

    /* Raise the packet number and the address counters (if there weren't any errors). */
    if (X_OK == status)
    {

        for (uint32_t i = total_size; i < size + total_size; i++)
        {
            buffer[i] = received_packet_data[i - total_size];
        }

        ledfasttoggle(2);

        xmodem_packet_number++;
        total_size += size;
        DataBufferTotalSize = total_size;
    }

    return status;
}

/**
 * @brief   Handles the xmodem error.
 *          Raises the error counter, then if the number of the errors reached critical, do a graceful abort, otherwise send a NAK.
 * @param   *error_number:    Number of current errors (passed as a pointer).
 * @param   max_error_number: Maximal allowed number of errors.
 * @return  status: X_ERROR in case of too many errors, X_OK otherwise.
 */

static xmodem_status xmodem_error_handler(uint8_t *error_number, uint8_t max_error_number, uint8_t (*rx)(uint8_t *, uint16_t, uint32_t), uint8_t (*tx)(uint8_t, uint32_t))
{
    xmodem_status status = X_OK;
    /* Raise the error counter. */
    (*error_number)++;
    /* If the counter reached the max value, then abort. */
    if ((*error_number) >= max_error_number)
    {
        /* Graceful abort. */
        (void)tx(X_CAN, PROTOCOL_TIMEOUT);
        (void)tx(X_CAN, PROTOCOL_TIMEOUT);
        status = X_ERROR;
    }
    /* Otherwise send a NAK for a repeat. */
    else
    {
        (void)tx(X_NAK, PROTOCOL_TIMEOUT);
        status = X_OK;
    }
    return status;
}