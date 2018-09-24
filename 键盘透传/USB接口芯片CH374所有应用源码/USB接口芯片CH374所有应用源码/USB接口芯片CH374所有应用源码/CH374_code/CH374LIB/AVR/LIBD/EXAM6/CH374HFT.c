/* 2004.06.05
****************************************
**  Copyright  (C)  W.ch  1999-2004   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  USB Host File Interface for CH374 **
**  TC2.0@PC, WinAVR_GCC_3.45@AVR     **
****************************************
*/
/* CH375 �����ļ�ϵͳ�ӿ� */
/* ֧��: FAT12/FAT16/FAT32 */

/* AVR��Ƭ��C���Ե�U���ļ���дʾ������ */
/* �ó���U���е�/C51/CH375HFT.C�ļ��е�ǰ600���ַ���ʾ����,
   ����Ҳ���ԭ�ļ�CH375HFT.C, ��ô�ó�����ʾC51��Ŀ¼��������CH375��ͷ���ļ���,
   ����Ҳ���C51��Ŀ¼, ��ô�ó�����ʾ��Ŀ¼�µ������ļ���,
   ��󽫳���ROM�е�һ���ַ���д��д���½����ļ�"NEWFILE.TXT"��
*/
/* CH375��INT#���Ų��ò�ѯ��ʽ����, ���ݸ��Ʒ�ʽΪ"�ڲ�����", ������������ATmega128��Ƭ��, ����0��������Ϣ,9600bps */
/* �������ֽ�Ϊ��λ��дU���ļ�,��д�ٶȽ�����ģʽ��,���������ֽ�ģʽ��д�ļ�����Ҫ�ļ����ݻ�����FILE_DATA_BUF,
   �����ܹ�ֻ��Ҫ600�ֽڵ�RAM,�����ڵ�Ƭ��Ӳ����Դ���ޡ�������С���Ҷ�д�ٶ�Ҫ�󲻸ߵ�ϵͳ */

/* AVR-GCC -v -mmcu=atmega128 -O2 -xc CH375HFT.c -oCH375HFT.ELF -lCH375HFD */
/* AVR-OBJCOPY -j .text -j .data -O ihex CH375HFT.ELF CH375HFT.HEX */

#include <string.h>
#include <stdio.h>
#define		F_CPU	16000000		/* ��Ƭ����ƵΪ16MHz,������ʱ�ӳ��� */
#include <util/delay.h>
#include <avr/io.h>

/* ���¶������ϸ˵���뿴CH374HF9.H�ļ� */
#define LIB_CFG_INT_EN			0		/* CH374��INT#�������ӷ�ʽ,0Ϊ"��ѯ��ʽ",1Ϊ"�жϷ�ʽ" */

#define CH374_INT_WIRE			( PINB & 0x10 )	/* PINB.4, CH374���ж���INT#����,����CH374��INT#����,���ڲ�ѯ�ж�״̬ */
/* ���δ����CH374���ж�����,��ôӦ��ȥ����������,�Զ�ʹ�üĴ�����ѯ��ʽ */

//#define DISK_BASE_BUF_LEN		2048	/* Ĭ�ϵĴ������ݻ�������СΪ512�ֽ�,����ѡ��Ϊ2048����4096��֧��ĳЩ��������U��,Ϊ0���ֹ��.H�ļ��ж��建��������Ӧ�ó�����pDISK_BASE_BUF��ָ�� */
/* �����Ҫ���ô������ݻ������Խ�ԼRAM,��ô�ɽ�DISK_BASE_BUF_LEN����Ϊ0�Խ�ֹ��.H�ļ��ж��建����,����Ӧ�ó����ڵ���CH375Init֮ǰ��������������õĻ�������ʼ��ַ����pDISK_BASE_BUF���� */

#define NO_DEFAULT_CH374_F_ENUM		1		/* δ����CH374FileEnumer����ʽ�ֹ�Խ�Լ���� */
#define NO_DEFAULT_CH374_F_QUERY	1		/* δ����CH374FileQuery����ʽ�ֹ�Խ�Լ���� */
#define NO_DEFAULT_CH374_RESET		1		/* δ����CH374Reset����ʽ�ֹ�Խ�Լ���� */

#include "..\CH374HFD.H"

/* ��ЩAVR��Ƭ���ṩ����ϵͳ����,��ôֱ�ӽ�CH374������ϵͳ������,��8λI/O��ʽ���ж�д */
/* ��ȻAtmega128�ṩϵͳ����,���������ٶ�������ϵͳ����,������I/O����ģ�����CH374�Ĳ��ڶ�дʱ�� */
/* �����е�Ӳ�����ӷ�ʽ����(ʵ��Ӧ�õ�·���Բ����޸�����3�����ڶ�д�ӳ���) */
/*    ��Ƭ��������     CH374оƬ������
       PINB.4                INT#
       PORTB.3               A0
       PORTB.2               CS#
       PORTB.1               WR#
       PORTB.0               RD#
      PORTA(8λ�˿�)        D7-D0       */

void CH374_PORT_INIT( )  /* ����ʹ��ͨ��I/Oģ�Ⲣ�ڶ�дʱ��,���Խ��г�ʼ�� */
{
	DDRA = 0x00;  /* ����8λ����Ϊ���� */
	PORTB = 0x07;  /* ����CS,WR,RDĬ��Ϊ�ߵ�ƽ */
	DDRB = 0x0F;  /* ����CS,WR,RD,A0Ϊ���,����INT#Ϊ���� */
}

void Write374Index( UINT8 mCmd )		/* �������˿�д��������ַ */
{
/*	*(volatile unsigned char *)CH374_IDX_PORT_ADDR = mCmd;  ͨ������ֱ�Ӷ�дCH374������ͨI/Oģ�� */
	PORTB |= 0x08;  /* ���A0=1 */
	PORTA = mCmd;  /* ��CH374�Ĳ���������� */
	DDRA = 0xFF;  /* ����D0-D7��� */
	PORTB &= 0xF9;  /* �����Чд�����ź�, дCH374оƬ������˿�, A0=1; CS=0; WR=0; RD=1; */
	DDRA = 0xFF;  /* �ò���������,������ʱ,CH374Ҫ���д�����ȴ���100nS */
	PORTB |= 0x07;  /* �����Ч�Ŀ����ź�, ��ɲ���CH374оƬ, A0=1; CS=1; WR=1; RD=1; */
	DDRA = 0x00;  /* ��ֹ������� */
	PORTB &= 0xF7;  /* ���A0=0; ��ѡ���� */
}

void Write374Data( UINT8 mData )		/* �����ݶ˿�д������,������ַ�Զ���1 */
{
/*	*(volatile unsigned char *)CH374_DAT_PORT_ADDR = mData;  ͨ������ֱ�Ӷ�дCH374������ͨI/Oģ�� */
	PORTA = mData;  /* ��CH374�Ĳ���������� */
	DDRA = 0xFF;  /* ����D0-D7��� */
	PORTB &= 0xF1;  /* �����Чд�����ź�, дCH374оƬ�����ݶ˿�, A0=0; CS=0; WR=0; RD=1; */
	DDRA = 0xFF;  /* �ò���������,������ʱ,CH374Ҫ���д�����ȴ���100nS */
	PORTB |= 0x07;  /* �����Ч�Ŀ����ź�, ��ɲ���CH374оƬ, A0=0; CS=1; WR=1; RD=1; */
	DDRA = 0x00;  /* ��ֹ������� */
}

UINT8 Read374Data( void )			/* �����ݶ˿ڶ�������,������ַ�Զ���1 */
{
	UINT8	mData;
/*	mData = *(volatile unsigned char *)CH374_DAT_PORT_ADDR;  ͨ������ֱ�Ӷ�дCH374������ͨI/Oģ�� */
	DDRA = 0x00;  /* �������� */
	PORTB &= 0xF2;  /* �����Ч�������ź�, ��CH374оƬ�����ݶ˿�, A0=0; CS=0; WR=1; RD=0; */
	DDRA = 0x00;  /* �ò���������,������ʱ,CH374Ҫ���д�����ȴ���100nS */
	mData = PINA;  /* ��CH374�Ĳ���PA�������� */
	PORTB |= 0x07;  /* �����Ч�Ŀ����ź�, ��ɲ���CH374оƬ, A0=0; CS=1; WR=1; RD=1; */
	return( mData );
}

#include "PARA.C"	/* ����ʱ�� */
/*#include "SPI.C"*/    /* SPIʱ�� */

/* ��PB.7����һ��LED���ڼ����ʾ����Ľ���,�͵�ƽLED�� */
#define LED_OUT_INIT( )		{ PORTB |= 0x80; DDRB |= 0x80; }	/* PORTB.7 �ߵ�ƽΪ������� */
#define LED_OUT_ACT( )		{ PORTB &= 0x7F; }	/* PORTB.7 �͵�ƽ����LED��ʾ */
#define LED_OUT_INACT( )	{ PORTB |= 0x80; }	/* PORTB.7 �͵�ƽ����LED��ʾ */

/* ������״̬,�����������ʾ������벢ͣ�� */
void	mStopIfError( UINT8 iError )
{
	if ( iError == ERR_SUCCESS ) return;  /* �����ɹ� */
	printf( "Error: %02X\n", (UINT16)iError );  /* ��ʾ���� */
/* ���������,Ӧ�÷����������Լ�CH374DiskStatus״̬,�������CH374DiskConnect��ѯ��ǰU���Ƿ�����,���U���ѶϿ���ô�����µȴ�U�̲����ٲ���,
   ��������Ĵ�����:
   1������һ��CH374DiskReady,�ɹ����������,����Open,Read/Write��,��CH374DiskReady�л��Զ�����CH374DiskConnect�������������
   2�����CH374DiskReady���ɹ�,��ôǿ�н�CH374DiskStatus��ΪDISK_CONNECT״̬,Ȼ���ͷ��ʼ����(�ȴ�U������CH374DiskConnect��CH374DiskReady��) */
	while ( 1 ) {
		LED_OUT_ACT( );  /* LED��˸ */
		CH374DelaymS( 100 );
		LED_OUT_INACT( );
		CH374DelaymS( 100 );
	}
}

/* Ϊprintf��getkey���������ʼ������ */
int		uart_putchar( char c, FILE *stream )
{
	if (c == '\n') uart_putchar( '\r', stream );
	while ( ( UCSR0A & ( 1 << UDRE ) ) == 0 );
	UDR0 = c;
	return 0;
}

FILE	uart_str = FDEV_SETUP_STREAM( uart_putchar, NULL, _FDEV_SETUP_WRITE );

void	mInitSTDIO( )
{
	UBRR0H = 0;
	UBRR0L = 103;  /* 9600bps@16MHz */
	UCSR0B = 0x18; /* BIT(RXEN) | BIT(TXEN); */
	UCSR0C = 0x06; /* BIT(UCSZ1) | BIT(UCSZ0); */
	stdout = stdin = &uart_str;
}

int	main( ) {
	UINT8	i, c;
	UINT16	TotalCount;
	UINT8	*pCodeStr;
	CH374_PORT_INIT( );
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* ������LED��һ����ʾ���� */
	CH374DelaymS( 100 );  /* ��ʱ100���� */
	LED_OUT_INACT( );
	mInitSTDIO( );  /* Ϊ���ü����ͨ�����ڼ����ʾ���� */
	printf( "Start\n" );

#if DISK_BASE_BUF_LEN == 0
	pDISK_BASE_BUF = &my_buffer[0];  /* ����.H�ļ��ж���CH374��ר�û�����,�����û�����ָ��ָ������Ӧ�ó���Ļ��������ں����Խ�ԼRAM */
#endif

	i = CH374LibInit( );  /* ��ʼ��CH374������CH374оƬ,�����ɹ�����0 */
	mStopIfError( i );
/* ������·��ʼ�� */

	while ( 1 ) {
		printf( "Wait Udisk\n" );
		while ( 1 ) {  /* ֧��USB-HUB,�����ӵĴ���ο�EXAM1��EXAM13 */
			CH374DelaymS( 10 );  /* û��ҪƵ����ѯ */
			if ( CH374DiskConnect( ) == ERR_SUCCESS ) break;  /* ��ѯ��ʽ: �������Ƿ����Ӳ����´���״̬,���سɹ�˵������ */
		}
		LED_OUT_ACT( );  /* LED�� */
		CH374DelaymS( 200 );  /* ��ʱ,��ѡ����,�е�USB�洢����Ҫ��ʮ�������ʱ */

/* ���U���Ƿ�׼����,��ЩU�̲���Ҫ��һ��,����ĳЩU�̱���Ҫִ����һ�����ܹ��� */
		for ( i = 0; i < 5; i ++ ) {  /* �е�U�����Ƿ���δ׼����,�������Ա����� */
			CH374DelaymS( 100 );
			printf( "Ready ?\n" );
			if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* ��ѯ�����Ƿ�׼���� */
		}
		if ( CH374DiskStatus <= DISK_CONNECT ) continue;  /* ֧��USB-HUB */

/* ��ѯ������������ */
/*		printf( "DiskSize\n" );
		i = CH374DiskSize( );  
		mStopIfError( i );
		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  ��ʾΪ��MBΪ��λ������
*/

/* ��ȡԭ�ļ� */
		printf( "Open\n" );
		strcpy( (char *)mCmdParam.Open.mPathName, "/C51/CH374HFT.C" );  /* �ļ���,���ļ���C51��Ŀ¼�� */
		i = CH374FileOpen( );  /* ���ļ� */
		if ( i == ERR_MISS_DIR || i == ERR_MISS_FILE ) {  /* û���ҵ��ļ� */
/* �г��ļ� */
			if ( i == ERR_MISS_DIR ) pCodeStr = (PUINT8)"/*";  /* C51��Ŀ¼���������г���Ŀ¼�µ��ļ� */
			else pCodeStr = (PUINT8)"/C51/CH374*";  /* CH374HFT.C�ļ����������г�\C51��Ŀ¼�µ���CH374��ͷ���ļ� */
			printf( "List file %s\n", pCodeStr );
			for ( c = 0; c < 254; c ++ ) {  /* �������ǰ254���ļ�,���Ҫ������255���ļ���ο�EXAM1��EXAM0 */
				strcpy( (char *)mCmdParam.Open.mPathName, (char *)pCodeStr );  /* �����ļ���,*Ϊͨ���,�����������ļ�������Ŀ¼ */
/* ����һ��ö�ٷ����ǣ����˴���c��Ϊ0xFF��Ȼ��������ö����Ŵ����CH374vFileSize�У��Ӷ�������������254���ļ�����0��2147483647�� */
				i = strlen( (char *)mCmdParam.Open.mPathName );
				mCmdParam.Open.mPathName[ i ] = c;  /* �����ַ������Ƚ��������滻Ϊ���������,��0��254 */
				i = CH374FileOpen( );  /* ���ļ�,����ļ����к���ͨ���*,��Ϊ�����ļ������� */
				if ( i == ERR_MISS_FILE ) break;  /* ��Ҳ��������ƥ����ļ�,�Ѿ�û��ƥ����ļ��� */
				if ( i == ERR_FOUND_NAME ) {  /* ��������ͨ�����ƥ����ļ���,�ļ�����������·������������� */
					printf( "  match file %03d#: %s\n", (unsigned int)c, mCmdParam.Open.mPathName );  /* ��ʾ��ź���������ƥ���ļ���������Ŀ¼�� */
					continue;  /* ����������һ��ƥ����ļ���,�´�����ʱ��Ż��1 */
				}
				else {  /* ���� */
					mStopIfError( i );
					break;
				}
			}
		}
		else {  /* �ҵ��ļ����߳��� */
			mStopIfError( i );
			TotalCount = 600;  /* ׼����ȡ�ܳ��� */
			printf( "���ļ��ж�����ǰ%d���ַ���:\n",TotalCount );
			while ( TotalCount ) {  /* ����ļ��Ƚϴ�,һ�ζ�����,�����ٵ���CH374ByteRead������ȡ,�ļ�ָ���Զ�����ƶ� */
				if ( TotalCount > MAX_BYTE_IO ) c = MAX_BYTE_IO;  /* ʣ�����ݽ϶�,���Ƶ��ζ�д�ĳ��Ȳ��ܳ��� sizeof( mCmdParam.ByteRead.mByteBuffer ) */
				else c = TotalCount;  /* ���ʣ����ֽ��� */
				mCmdParam.ByteRead.mByteCount = c;  /* ���������ʮ�ֽ����� */
				i = CH374ByteRead( );  /* ���ֽ�Ϊ��λ��ȡ���ݿ�,���ζ�д�ĳ��Ȳ��ܳ���MAX_BYTE_IO,�ڶ��ε���ʱ���Ÿղŵ����� */
				mStopIfError( i );
				TotalCount -= mCmdParam.ByteRead.mByteCount;  /* ����,��ȥ��ǰʵ���Ѿ��������ַ��� */
				for ( i=0; i!=mCmdParam.ByteRead.mByteCount; i++ ) printf( "%c", mCmdParam.ByteRead.mByteBuffer[i] );  /* ��ʾ�������ַ� */
				if ( mCmdParam.ByteRead.mByteCount < c ) {  /* ʵ�ʶ������ַ�������Ҫ��������ַ���,˵���Ѿ����ļ��Ľ�β */
					printf( "\n" );
					printf( "�ļ��Ѿ�����\n" );
					break;
				}
			}
/*	    ���ϣ����ָ��λ�ÿ�ʼ��д,�����ƶ��ļ�ָ��
		mCmdParam.ByteLocate.mByteOffset = 608;  �����ļ���ǰ608���ֽڿ�ʼ��д
		CH374ByteLocate( );
		mCmdParam.ByteRead.mByteCount = 5;  ��ȡ5���ֽ�
		CH374ByteRead( );   ֱ�Ӷ�ȡ�ļ��ĵ�608���ֽڵ�612���ֽ�����,ǰ608���ֽڱ�����

	    ���ϣ������������ӵ�ԭ�ļ���β��,�����ƶ��ļ�ָ��
		CH374FileOpen( );
		mCmdParam.ByteLocate.mByteOffset = 0xffffffff;  �Ƶ��ļ���β��
		CH374ByteLocate( );
		mCmdParam.ByteWrite.mByteCount = 13;  д��13���ֽڵ�����
		CH374ByteWrite( );   ��ԭ�ļ��ĺ����������,�¼ӵ�13���ֽڽ���ԭ�ļ���β������
		mCmdParam.ByteWrite.mByteCount = 2;  д��2���ֽڵ�����
		CH374ByteWrite( );   ������ԭ�ļ��ĺ����������
		mCmdParam.ByteWrite.mByteCount = 0;  д��0���ֽڵ�����,ʵ���ϸò�������֪ͨ���������ļ�����
		CH374ByteWrite( );   д��0�ֽڵ�����,�����Զ������ļ��ĳ���,�����ļ���������15,�����������,��ôִ��CH374FileCloseʱҲ���Զ������ļ�����
*/
			printf( "Close\n" );
			i = CH374FileClose( );  /* �ر��ļ� */
			mStopIfError( i );
		}

#ifdef EN_DISK_WRITE  /* �ӳ����֧��д���� */
/* �������ļ� */
		printf( "Create\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "/NEWFILE.TXT" );  /* ���ļ���,�ڸ�Ŀ¼��,�����ļ��� */
		i = CH374FileCreate( );  /* �½��ļ�����,����ļ��Ѿ���������ɾ�������½� */
		mStopIfError( i );
		printf( "Write\n" );
		pCodeStr = (PUINT8)"Note: \xd\xa������������ֽ�Ϊ��λ����U���ļ���д,��Ƭ��ֻ��Ҫ��600�ֽڵ�RAM\xd\xa";
		while( 1 ) {  /* �ֶ��д���ļ����� */
			for ( i=0; i<MAX_BYTE_IO; i++ ) {
				c = *pCodeStr;
				mCmdParam.ByteWrite.mByteBuffer[i] = c;
				if ( c == 0 ) break;  /* Դ�ַ������� */
				pCodeStr++;
			}
			if ( i == 0 ) break;  /* Դ�ַ�������,���д�ļ� */
			mCmdParam.ByteWrite.mByteCount = i;  /* д�����ݵ��ַ���,���ζ�д�ĳ��Ȳ��ܳ���MAX_BYTE_IO,�ڶ��ε���ʱ���Ÿղŵ����д */
			i = CH374ByteWrite( );  /* ���ļ�д������ */
			mStopIfError( i );
		}
/*		printf( "Modify\n" );
		mCmdParam.Modify.mFileAttr = 0xff;   �������: �µ��ļ�����,Ϊ0FFH���޸�
		mCmdParam.Modify.mFileTime = 0xffff;   �������: �µ��ļ�ʱ��,Ϊ0FFFFH���޸�,ʹ���½��ļ�������Ĭ��ʱ��
		mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  �������: �µ��ļ�����: 2004.05.18
		mCmdParam.Modify.mFileSize = 0xffffffff;   �������: �µ��ļ�����,���ֽ�Ϊ��λд�ļ�Ӧ���ɳ����ر��ļ�ʱ�Զ����³���,���Դ˴����޸�
		i = CH374FileModify( );   �޸ĵ�ǰ�ļ�����Ϣ,�޸�����
		mStopIfError( i );
*/
		printf( "Close\n" );
		mCmdParam.Close.mUpdateLen = 1;  /* �Զ������ļ�����,���ֽ�Ϊ��λд�ļ�,�����ó����ر��ļ��Ա��Զ������ļ����� */
		i = CH374FileClose( );
		mStopIfError( i );

/* ɾ��ĳ�ļ� */
/*		printf( "Erase\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "/OLD" );  ����ɾ�����ļ���,�ڸ�Ŀ¼��
		i = CH374FileErase( );  ɾ���ļ����ر�
		if ( i != ERR_SUCCESS ) printf( "Error: %02X\n", (UINT16)i );  ��ʾ����
*/

/* ��ѯ������Ϣ */
/*		printf( "Disk\n" );
		i = CH374DiskQuery( );
		mStopIfError( i );
		printf( "Fat=%d, Total=%ld, Free=%ld\n", (UINT16)mCmdParam.Query.mDiskFat, mCmdParam.Query.mTotalSector, mCmdParam.Query.mFreeSector );
*/
#endif
		printf( "Take out\n" );
		while ( 1 ) {  /* ֧��USB-HUB */
			CH374DelaymS( 10 );  /* û��ҪƵ����ѯ */
			if ( CH374DiskConnect( ) != ERR_SUCCESS ) break;  /* ��ѯ��ʽ: �������Ƿ����Ӳ����´���״̬,���سɹ�˵������ */
		}
		LED_OUT_INACT( );  /* LED�� */
		CH374DelaymS( 200 );
	}
}
