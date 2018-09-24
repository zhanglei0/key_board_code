/* ���ڽӿ��ӳ���,��CH374�ӳ������� */

UINT8	CH374_READ_REGISTER( UINT8 mAddr )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ���Ĵ�����ȡ���� */
{
	Write374Index( mAddr );
	return( Read374Data( ) );
}

void	CH374_WRITE_REGISTER( UINT8 mAddr, UINT8 mData )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ���Ĵ���д������ */
{
	Write374Index( mAddr );
	Write374Data( mData );
}

void	CH374_READ_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ����ʼ��ַ�������ݿ� */
{
	Write374Index( mAddr );
	while ( mLen -- ) *mBuf++ = Read374Data( );
}

void	CH374_WRITE_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ����ʼ��ַд�����ݿ� */
{
	Write374Index( mAddr );
	while ( mLen -- ) Write374Data( *mBuf++ );
}

PUINT8	CH374_READ_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��˫����������64�ֽڵ����ݿ�,���ص�ǰ��ַ */
{
	UINT8	i;
	Write374Index( mAddr );
	for ( i = CH374_BLOCK_SIZE / 4; i != 0; i -- ) {  /* ����ѭ������������΢����ٶ� */
		*mBuf = Read374Data( );
		mBuf ++;
		*mBuf = Read374Data( );
		mBuf ++;
		*mBuf = Read374Data( );
		mBuf ++;
		*mBuf = Read374Data( );
		mBuf ++;
	}
	return( mBuf );
}

PUINT8	CH374_WRITE_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��˫������д��64�ֽڵ����ݿ�,���ص�ǰ��ַ */
{
	UINT8	i;
	Write374Index( mAddr );
	for ( i = CH374_BLOCK_SIZE / 4; i != 0; i -- ) {  /* ����ѭ������������΢����ٶ� */
		Write374Data( *mBuf );
		mBuf ++;
		Write374Data( *mBuf );
		mBuf ++;
		Write374Data( *mBuf );
		mBuf ++;
		Write374Data( *mBuf );
		mBuf ++;
	}
	return( mBuf );
}

void	CH374_WRITE_BLOCK_C( UINT8 mLen, PUINT8C mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��RAM_HOST_TRANд�볣�������ݿ� */
{
	Write374Index( RAM_HOST_TRAN );
	do {
		Write374Data( *mBuf );
		mBuf ++;
	} while ( -- mLen );
}
