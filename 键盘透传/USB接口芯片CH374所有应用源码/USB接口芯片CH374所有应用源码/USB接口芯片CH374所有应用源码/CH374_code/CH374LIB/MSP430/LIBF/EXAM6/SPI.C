/* SPI�ӿ��ӳ���,��CH374�ӳ������� */

UINT8	CH374_READ_REGISTER( UINT8 mAddr )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ���Ĵ�����ȡ���� */
{
	UINT8	d;
	Spi374Start( mAddr, CMD_SPI_374READ );
	d = Spi374InByte( );
	Spi374Stop( );
	return( d );
}

void	CH374_WRITE_REGISTER( UINT8 mAddr, UINT8 mData )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ���Ĵ���д������ */
{
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	Spi374OutByte( mData );
	Spi374Stop( );
}

void	CH374_READ_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ����ʼ��ַ�������ݿ� */
{
	Spi374Start( mAddr, CMD_SPI_374READ );
	while ( mLen -- ) *mBuf++ = Spi374InByte( );
	Spi374Stop( );
}

void	CH374_WRITE_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��ָ����ʼ��ַд�����ݿ� */
{
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	while ( mLen -- ) Spi374OutByte( *mBuf++ );
	Spi374Stop( );
}

PUINT8	CH374_READ_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��˫����������64�ֽڵ����ݿ�,���ص�ǰ��ַ */
{
	UINT8	i;
	Spi374Start( mAddr, CMD_SPI_374READ );
	for ( i = CH374_BLOCK_SIZE; i != 0; i -- ) *mBuf++ = Spi374InByte( );
	Spi374Stop( );
	return( mBuf );
}

PUINT8	CH374_WRITE_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��˫������д��64�ֽڵ����ݿ�,���ص�ǰ��ַ */
{
	UINT8	i;
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	for ( i = CH374_BLOCK_SIZE; i != 0; i -- ) Spi374OutByte( *mBuf++ );
	Spi374Stop( );
	return( mBuf );
}

void	CH374_WRITE_BLOCK_C( UINT8 mLen, PUINT8C mBuf )  /* �ⲿ����ı�CH374�������õ��ӳ���,��RAM_HOST_TRANд�볣�������ݿ� */
{
	Spi374Start( RAM_HOST_TRAN, CMD_SPI_374WRITE );
	do {
		Spi374OutByte( *mBuf );
		mBuf ++;
	} while ( -- mLen );
	Spi374Stop( );
}
