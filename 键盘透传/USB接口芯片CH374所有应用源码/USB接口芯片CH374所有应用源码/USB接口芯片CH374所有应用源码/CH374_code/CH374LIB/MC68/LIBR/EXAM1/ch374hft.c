/* 2004.06.05
****************************************
**  Copyright  (C)  W.ch  1999-2004   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  USB Host File Interface for CH374 **
**  TC2.0@PC, gcc 2.95.3@MC68         **
****************************************
*/
/* CH374 �����ļ�ϵͳ�ӿ� */
/* ֧��: FAT12/FAT16/FAT32 */

/* MC68��Ƭ��C���Ե�U���ļ���дʾ������ */
/* �ó���U���е�/C51/CH374HFT.C�ļ��е�Сд��ĸת�ɴ�д��ĸ��, д���½����ļ�NEWFILE.TXT��,
   ����Ҳ���ԭ�ļ�CH374HFT.C, ��ô�ó�����ʾC51��Ŀ¼��������CH374��ͷ���ļ���, ���½�NEWFILE.TXT�ļ���д����ʾ��Ϣ,
   ����Ҳ���C51��Ŀ¼, ��ô�ó�����ʾ��Ŀ¼�µ������ļ���, ���½�NEWFILE.TXT�ļ���д����ʾ��Ϣ
*/
/* CH374��INT#���Ų��ò�ѯ��ʽ����, ���ݸ��Ʒ�ʽΪ"�ڲ�����", ������������MC68��Ƭ��uClinux */

/* m68k-elf-gcc -m68000 ??... */

#include <stdio.h>
#include <string.h>

/* ���¶������ϸ˵���뿴CH374HFR.H�ļ� */
#define LIB_CFG_INT_EN			0		/* CH374��INT#�������ӷ�ʽ,0Ϊ"��ѯ��ʽ",1Ϊ"�жϷ�ʽ" */

#define CH374_INT_WIRE			( *(UINT8V *)0xFFFFF419 & 0x02 )	/* CH374���ж���INT#����,����CH374��INT#����,���ڲ�ѯ�ж�״̬ */
/* ���δ����CH374���ж�����,��ôӦ��ȥ����������,�Զ�ʹ�üĴ�����ѯ��ʽ */

#define DISK_BASE_BUF_LEN		2048	/* Ĭ�ϵĴ������ݻ�������СΪ512�ֽ�,����ѡ��Ϊ2048����4096��֧��ĳЩ��������U��,Ϊ0���ֹ��.H�ļ��ж��建��������Ӧ�ó�����pDISK_BASE_BUF��ָ�� */
/* �����Ҫ���ô������ݻ������Խ�ԼRAM,��ô�ɽ�DISK_BASE_BUF_LEN����Ϊ0�Խ�ֹ��.H�ļ��ж��建����,����Ӧ�ó����ڵ���CH375Init֮ǰ��������������õĻ�������ʼ��ַ����pDISK_BASE_BUF���� */

#define NO_DEFAULT_CH374_F_ENUM		1		/* δ����CH374FileEnumer����ʽ�ֹ�Խ�Լ���� */
#define NO_DEFAULT_CH374_F_QUERY	1		/* δ����CH374FileQuery����ʽ�ֹ�Խ�Լ���� */
#define NO_DEFAULT_CH374_RESET		1		/* δ����CH374Reset����ʽ�ֹ�Խ�Լ���� */

#include "../CH374HFR.h"

UINT8	my_buffer[ 0x2000 ];			/* �ⲿRAM���ļ����ݻ����� */
/* ���׼��ʹ��˫�����������д,��ô�����ڲ�����ָ����������ַ */

#define CH374_IDX_PORT	(*(UINT8V *)0x1000001E)		/* CH374�����˿ڵ�I/O��ַ */
#define CH374_DAT_PORT	(*(UINT8V *)0x1000001C)		/* CH374���ݶ˿ڵ�I/O��ַ */

void CH374_PORT_INIT( )  /* ����ʹ�ñ�׼���ڶ�дʱ��,���������ʼ�� */
{
}

#define	Write374Index( a )	{ CH374_IDX_PORT = a; }	/* �������˿�д��������ַ */
//static	void Write374Index( UINT8 mIndex )  /* ��CH374д������ַ */
//{
//	CH374_IDX_PORT = mIndex;
//}

#define	Write374Data( d )	{ CH374_DAT_PORT = d; }	/* �����ݶ˿�д������,������ַ�Զ���1 */
//static	void Write374Data( UINT8 mData )  /* ��CH374д���� */
//{
//	CH374_DAT_PORT = mData;
//}

#define	Read374Data( )		( CH374_DAT_PORT )		/* �����ݶ˿ڶ�������,������ַ�Զ���1 */
//static	UINT8 Read374Data( void )  /* ��CH374������ */
//{
//	return( CH374_DAT_PORT );
//}

#include "para.c"	/* ����ʱ�� */
/*#include "spi.c"*/    /* SPIʱ�� */

/* ��PIN����һ��LED���ڼ����ʾ����Ľ���,�͵�ƽLED�� */
#define LED_OUT_INIT( )		{  }	/* �ߵ�ƽΪ������� */
#define LED_OUT_ACT( )		{  }	/* �͵�ƽ����LED��ʾ */
#define LED_OUT_INACT( )	{  }	/* �͵�ƽ����LED��ʾ */

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

int	main( ) {
	UINT8	i, c, SecCount;
	UINT16	NewSize, count;  /* ��Ϊ��ʾ���RAM����ֻ��32KB,����NewSize����Ϊ16λ,ʵ��������ļ�����32256�ֽ�,Ӧ�÷ּ��ζ�д���ҽ�NewSize��ΪUINT32�Ա��ۼ� */
	PUINT8	pCodeStr;
	CH374_PORT_INIT( );
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* ������LED��һ����ʾ���� */
	CH374DelaymS( 100 );  /* ��ʱ100���� */
	LED_OUT_INACT( );
	printf( "Start\n" );

#if DISK_BASE_BUF_LEN == 0
	pDISK_BASE_BUF = &my_buffer[0];  /* ����.H�ļ��ж���CH374��ר�û�����,�����û�����ָ��ָ������Ӧ�ó���Ļ��������ں����Խ�ԼRAM */
#endif

	i = CH374LibInit( );  /* ��ʼ��CH374������CH374оƬ,�����ɹ�����0 */
	mStopIfError( i );
/* ������·��ʼ�� */

	while ( 1 ) {
		printf( "Wait Udisk\n" );

#ifdef UNSUPPORT_USB_HUB
/* �������Ҫ֧��USB-HUB,��ô�ȴ�U�̲���ĳ�����CH374����,����ͨ��CH374DiskConnect��ѯ����,��������ͨ��CH374DiskReady�ȴ�����,Ȼ���д */
		while ( CH374DiskStatus < DISK_CONNECT ) {  /* ��ѯCH374�жϲ������ж�״̬,�ȴ�U�̲��� */
			CH374DiskConnect( );
			CH374DelaymS( 50 );  /* û��ҪƵ����ѯ */
		}
		LED_OUT_ACT( );  /* LED�� */
		CH374DelaymS( 200 );  /* ��ʱ,��ѡ����,�е�USB�洢����Ҫ��ʮ�������ʱ */

/* ���ڼ�⵽USB�豸��,���ȴ�100*50mS,��Ҫ�����ЩMP3̫��,���ڼ�⵽USB�豸��������DISK_MOUNTED��,���ȴ�5*50mS,��Ҫ���DiskReady������ */
		for ( i = 0; i < 100; i ++ ) {  /* ��ȴ�ʱ��,100*50mS */
			CH374DelaymS( 50 );
			printf( "Ready ?\n" );
			if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* ��ѯ�����Ƿ�׼���� */
			if ( CH374DiskStatus < DISK_CONNECT ) break;  /* ��⵽�Ͽ�,���¼�Ⲣ��ʱ */
			if ( CH374DiskStatus >= DISK_MOUNTED && i > 5 ) break;  /* �е�U�����Ƿ���δ׼����,�������Ժ���,ֻҪ�佨������MOUNTED�ҳ���5*50mS */
		}
		if ( CH374DiskStatus < DISK_CONNECT ) {  /* ��⵽�Ͽ�,���¼�Ⲣ��ʱ */
			printf( "Device gone\n" );
			continue;  /* ���µȴ� */
		}
		if ( CH374DiskStatus < DISK_MOUNTED ) {  /* δ֪USB�豸,����USB���̡���ӡ���� */
			printf( "Unknown device\n" );
			goto UnknownUsbDevice;
		}
#else
/* �����Ҫ֧��USB-HUB,��ô����ο�����������ĵȴ����� */
		while ( 1 ) {  /* ֧��USB-HUB */
			CH374DelaymS( 50 );  /* û��ҪƵ����ѯ */
			if ( CH374DiskConnect( ) == ERR_SUCCESS ) {  /* ��ѯ��ʽ: �������Ƿ����Ӳ����´���״̬,���سɹ�˵������ */
				CH374DelaymS( 200 );  /* ��ʱ,��ѡ����,�е�USB�洢����Ҫ��ʮ�������ʱ */

/* ���ڼ�⵽USB�豸��,���ȴ�100*50mS,��Ҫ�����ЩMP3̫��,���ڼ�⵽USB�豸��������DISK_MOUNTED��,���ȴ�5*50mS,��Ҫ���DiskReady������ */
				for ( i = 0; i < 100; i ++ ) {  /* ��ȴ�ʱ��,100*50mS */
					CH374DelaymS( 50 );
					printf( "Ready ?\n" );
					if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* ��ѯ�����Ƿ�׼���� */
					if ( CH374DiskStatus < DISK_CONNECT ) {  /* ��⵽�Ͽ�,���¼�Ⲣ��ʱ */
						printf( "Device gone\n" );
						break;  /* ���µȴ� */
					}
					if ( CH374DiskStatus >= DISK_MOUNTED && i > 5 ) break;  /* �е�U�����Ƿ���δ׼����,�������Ժ���,ֻҪ�佨������MOUNTED�ҳ���5*50mS */
					if ( CH374DiskStatus == DISK_CONNECT ) {  /* ���豸���� */
						if ( CH374vHubPortCount ) {  /* ������һ��USB-HUB,������û��U�� */
							printf( "No Udisk in USB_HUB\n" );
							break;
						}
						else {  /* δ֪USB�豸,�п�����U�̷�Ӧ̫��,����Ҫ������ */
						}
					}
				}
				if ( CH374DiskStatus >= DISK_MOUNTED ) {  /* ��U�� */
					break;  /* ��ʼ����U�� */
				}
				if ( CH374DiskStatus == DISK_CONNECT ) {  /* ��γ��Ի��ǲ���,���Ʋ���U�� */
					if ( CH374vHubPortCount ) {  /* ������һ��USB-HUB,������û��U�� */
/* ��while�еȴ�HUB�˿���U�� */
					}
					else {  /* δ֪USB�豸,����USB���̡���ӡ����,�����Ѿ����˺ܶ�λ����� */
						printf( "Unknown device\n" );
						goto UnknownUsbDevice;
					}
				}
			}
		}
		LED_OUT_ACT( );  /* LED�� */
#endif

#if DISK_BASE_BUF_LEN
		if ( DISK_BASE_BUF_LEN < CH374vSectorSize ) {  /* ���������ݻ������Ƿ��㹻��,CH374vSectorSize��U�̵�ʵ��������С */
			printf( "Too large sector size\n" );
			goto UnknownUsbDevice;
		}
#endif

/* ��ѯ������������ */
/*		printf( "DiskSize\n" );
		i = CH374DiskSize( );  
		mStopIfError( i );
		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  ��ʾΪ��MBΪ��λ������
*/
/* ��ȡԭ�ļ� */
		printf( "Open\n" );
		strcpy( (char *)mCmdParam.Open.mPathName, "\\C51\\CH374HFT.C" );  /* �ļ���,���ļ���C51��Ŀ¼�� */
		i = CH374FileOpen( );  /* ���ļ� */
		if ( i == ERR_MISS_DIR || i == ERR_MISS_FILE ) {  /* û���ҵ��ļ� */
/* �г��ļ� */
			if ( i == ERR_MISS_DIR ) pCodeStr = (PUINT8)"\\*";  /* C51��Ŀ¼���������г���Ŀ¼�µ��ļ� */
			else pCodeStr = (PUINT8)"\\C51\\CH*";  /* CH374HFT.C�ļ����������г�\C51��Ŀ¼�µ���CH��ͷ���ļ� */
			printf( "List file %s\n", pCodeStr );
			for ( count = 0; count < 10000; count ++ ) {  /* �������ǰ10000���ļ�,ʵ����û������ */
				strcpy( (char *)mCmdParam.Open.mPathName, (char *)pCodeStr );  /* �����ļ���,*Ϊͨ���,�����������ļ�������Ŀ¼ */
				i = strlen( (char *)mCmdParam.Open.mPathName );
				mCmdParam.Open.mPathName[ i ] = 0xFF;  /* �����ַ������Ƚ��������滻Ϊ���������,��0��254,�����0xFF��255��˵�����������CH374vFileSize������ */
				CH374vFileSize = count;  /* ָ������/ö�ٵ���� */
				i = CH374FileOpen( );  /* ���ļ�,����ļ����к���ͨ���*,��Ϊ�����ļ������� */
/* CH374FileEnum �� CH374FileOpen ��Ψһ�����ǵ����߷���ERR_FOUND_NAMEʱ��ô��Ӧ��ǰ�߷���ERR_SUCCESS */
				if ( i == ERR_MISS_FILE ) break;  /* ��Ҳ��������ƥ����ļ�,�Ѿ�û��ƥ����ļ��� */
				if ( i == ERR_FOUND_NAME ) {  /* ��������ͨ�����ƥ����ļ���,�ļ�����������·������������� */
					printf( "  match file %04d#: %s\n", (unsigned int)count, mCmdParam.Open.mPathName );  /* ��ʾ��ź���������ƥ���ļ���������Ŀ¼�� */
					continue;  /* ����������һ��ƥ����ļ���,�´�����ʱ��Ż��1 */
				}
				else {  /* ���� */
					mStopIfError( i );
					break;
				}
			}
			pCodeStr = (PUINT8)"�Ҳ���/C51/CH374HFT.C�ļ�\xd\n";
			for ( i = 0; i != 255; i ++ ) {
				if ( ( my_buffer[i] = *pCodeStr ) == 0 ) break;
				pCodeStr++;
			}
			NewSize = i;  /* ���ļ��ĳ��� */
			SecCount = 1;  /* (NewSize+CH374vSectorSize-1)/CH374vSectorSize, �����ļ���������,��Ϊ��д��������Ϊ��λ�� */
		}
		else {  /* �ҵ��ļ����߳��� */
			mStopIfError( i );
/*			printf( "Query\n" );
			i = CH374FileQuery( );  ��ѯ��ǰ�ļ�����Ϣ
			mStopIfError( i );*/
			printf( "Read\n" );
			if ( CH374vFileSize > sizeof( my_buffer ) ) {  /* ԭ�ļ����ȴ��ڻ���������,һ�ζ�ȡ���� */
				SecCount = sizeof( my_buffer ) / CH374vSectorSize;  /* ������ʾ���õ�62256ֻ��32K�ֽ�,����CH374�ӳ�����CH374vSectorSize���ֽ�,����ֻ��ȡ������60������,Ҳ���ǲ�����30K�ֽ� */
				NewSize = sizeof( my_buffer );  /* ����RAM�����������Ƴ��� */
			}
			else {  /* ���ԭ�ļ���С,��ôʹ��ԭ���� */
				SecCount = ( CH374vFileSize + CH374vSectorSize - 1 ) / CH374vSectorSize;  /* �����ļ���������,��Ϊ��д��������Ϊ��λ��,�ȼ�CH374vSectorSize-1��Ϊ�˶����ļ�β������1�������Ĳ��� */
				NewSize = (UINT16)CH374vFileSize;  /* ԭ�ļ��ĳ��� */
			}
			printf( "Size=%ld, Len=%d, Sec=%d\n", CH374vFileSize, NewSize, (UINT16)SecCount );
			mCmdParam.ReadX.mSectorCount = SecCount;  /* ��ȡȫ������,�������60��������ֻ��ȡ60������ */
			mCmdParam.ReadX.mDataBuffer = &my_buffer[0];  /* ָ���ļ����ݻ���������ʼ��ַ */
			CH374vFileSize += CH374vSectorSize - 1;  /* Ĭ�������,��������ʽ��ȡ����ʱ,�޷������ļ�β������1�������Ĳ���,���Ա�����ʱ�Ӵ��ļ������Զ�ȡβ����ͷ */
			i = CH374FileReadX( );  /* ���ļ���ȡ���� */
			CH374vFileSize -= CH374vSectorSize - 1;  /* �ָ�ԭ�ļ����� */
			mStopIfError( i );
/*
		����ļ��Ƚϴ�,һ�ζ�����,�����ٵ���CH374FileReadX������ȡ,�ļ�ָ���Զ�����ƶ�
		while ( 1 ) {
			c = 32;   ÿ�ζ�ȡ32������
			mCmdParam.ReadX.mSectorCount = c;   ָ����ȡ��������
			mCmdParam.ReadX.mDataBuffer = &my_buffer[0];  ָ���ļ����ݻ���������ʼ��ַ
			CH374FileReadX();   ������ļ�ָ���Զ�����
			��������
			if ( mCmdParam.ReadX.mSectorCount < c ) break;   ʵ�ʶ�������������С��˵���ļ��Ѿ�����
		}

	    ���ϣ����ָ��λ�ÿ�ʼ��д,�����ƶ��ļ�ָ��
		mCmdParam.Locate.mSectorOffset = 3;  �����ļ���ǰ3��������ʼ��д
		i = CH374FileLocate( );
		mCmdParam.ReadX.mSectorCount = 10;
		mCmdParam.ReadX.mDataBuffer = &my_buffer[0];  ָ���ļ����ݻ���������ʼ��ַ
		CH374FileReadX();   ֱ�Ӷ�ȡ���ļ��ĵ�(CH374vSectorSize*3)���ֽڿ�ʼ������,ǰ3������������

	    ���ϣ������������ӵ�ԭ�ļ���β��,�����ƶ��ļ�ָ��
		i = CH374FileOpen( );
		mCmdParam.Locate.mSectorOffset = 0xffffffff;  �Ƶ��ļ���β��,������Ϊ��λ,���ԭ�ļ���3�ֽ�,���CH374vSectorSize���ֽڴ���ʼ���
		i = CH374FileLocate( );
		mCmdParam.WriteX.mSectorCount = 10;
		mCmdParam.WriteX.mDataBuffer = &my_buffer[0];
		CH374FileWriteX();   ��ԭ�ļ��ĺ����������

ʹ��CH374FileReadX�������ж������ݻ���������ʼ��ַ
		mCmdParam.ReadX.mSectorCount = 2;
		mCmdParam.ReadX.mDataBuffer = 0x2000;  �����������ݷŵ�2000H��ʼ�Ļ�������
		CH374FileReadX();   ���ļ��ж�ȡ2��������ָ��������

ʹ��CH374FileWriteX�������ж������ݻ���������ʼ��ַ
		mCmdParam.WiiteX.mSectorCount = 2;
		mCmdParam.WriteX.mDataBuffer = 0x4600;  ��4600H��ʼ�Ļ������е�����д��
		CH374FileWriteX();   ��ָ���������е�����д��2���������ļ���

*/
			printf( "Close\n" );
			i = CH374FileClose( );  /* �ر��ļ� */
			mStopIfError( i );

			i = my_buffer[100];
			my_buffer[100] = 0;  /* ���ַ���������־,�����ʾ100���ַ� */
			printf( "Line 1: %s\n", my_buffer );
			my_buffer[100] = i;  /* �ָ�ԭ�ַ� */
			for ( count=0; count < NewSize; count ++ ) {  /* ���ļ��е�Сд�ַ�ת��Ϊ��д */
				c = my_buffer[ count ];
				if ( c >= 'a' && c <= 'z' ) my_buffer[ count ] = c - ( 'a' - 'A' );
			}
		}

#ifdef EN_DISK_WRITE  /* �ӳ����֧��д���� */
/* �������ļ� */
		printf( "Create\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "\\NEWFILE.TXT" );  /* ���ļ���,�ڸ�Ŀ¼�� */
		i = CH374FileCreate( );  /* �½��ļ�����,����ļ��Ѿ���������ɾ�������½� */
		mStopIfError( i );
		printf( "Write\n" );
		mCmdParam.WriteX.mSectorCount = SecCount;  /* д���������������� */
		mCmdParam.WriteX.mDataBuffer = &my_buffer[0];  /* ָ���ļ����ݻ���������ʼ��ַ */
		i = CH374FileWriteX( );  /* ���ļ�д������ */
		mStopIfError( i );
/* Ĭ�������,���������mCmdParam.WriteX.mSectorCount��Ϊ0��ôCH374FileWriteXֻ����д�����ݶ����޸��ļ�����,
   �����ʱ�䲻д��������Ӧ�ø����ļ�����,��ֹͻȻ�ϵ��ǰ��д����������ļ����Ȳ����,
   �����Ҫд�����ݺ������޸�/�����ļ�����,��ô������������mCmdParam.WriteX.mSectorCountΪ0�����CH374FileWriteXǿ�и����ļ�����,
   ���ȷ������ͻȻ�ϵ���ߺ���ܿ������ݲ���д���򲻱ظ����ļ�����,��������ٶȲ�����U�����(U���ڲ����ڴ���������,����Ƶ����д) */
		printf( "Modify\n" );
		mCmdParam.Modify.mFileAttr = 0xff;  /* �������: �µ��ļ�����,Ϊ0FFH���޸� */
		mCmdParam.Modify.mFileTime = 0xffff;  /* �������: �µ��ļ�ʱ��,Ϊ0FFFFH���޸�,ʹ���½��ļ�������Ĭ��ʱ�� */
		mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  /* �������: �µ��ļ�����: 2004.05.18 */
		mCmdParam.Modify.mFileSize = NewSize;  /* �������: ���ԭ�ļ���С,��ô�µ��ļ�������ԭ�ļ�һ����,����RAM����,����ļ����ȴ���64KB,��ôNewSize����ΪUINT32 */
		i = CH374FileModify( );  /* �޸ĵ�ǰ�ļ�����Ϣ,�޸����ںͳ��� */
		mStopIfError( i );
		printf( "Close\n" );
		mCmdParam.Close.mUpdateLen = 0;  /* ��Ҫ�Զ������ļ�����,����Զ�����,��ô�ó�������CH374vSectorSize�ı��� */
		i = CH374FileClose( );
		mStopIfError( i );

/* ɾ��ĳ�ļ� */
/*		printf( "Erase\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "\\OLD" );  ����ɾ�����ļ���,�ڸ�Ŀ¼��
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
UnknownUsbDevice:
		printf( "Take out\n" );
		while ( 1 ) {  /* ֧��USB-HUB */
			CH374DelaymS( 10 );  /* û��ҪƵ����ѯ */
			if ( CH374DiskConnect( ) != ERR_SUCCESS ) break;  /* ��ѯ��ʽ: �������Ƿ����Ӳ����´���״̬,���سɹ�˵������ */
		}
		LED_OUT_INACT( );  /* LED�� */
		CH374DelaymS( 200 );
	}
}
