/* SPI接口子程序,供CH374子程序库调用 */

UINT8	CH374_READ_REGISTER( UINT8 mAddr )  /* 外部定义的被CH374程序库调用的子程序,从指定寄存器读取数据 */
{
	UINT8	d;
	Spi374Start( mAddr, CMD_SPI_374READ );
	d = Spi374InByte( );
	Spi374Stop( );
	return( d );
}

void	CH374_WRITE_REGISTER( UINT8 mAddr, UINT8 mData )  /* 外部定义的被CH374程序库调用的子程序,向指定寄存器写入数据 */
{
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	Spi374OutByte( mData );
	Spi374Stop( );
}

void	CH374_READ_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,从指定起始地址读出数据块 */
{
	Spi374Start( mAddr, CMD_SPI_374READ );
	while ( mLen -- ) *mBuf++ = Spi374InByte( );
	Spi374Stop( );
}

void	CH374_WRITE_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,向指定起始地址写入数据块 */
{
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	while ( mLen -- ) Spi374OutByte( *mBuf++ );
	Spi374Stop( );
}

PUINT8	CH374_READ_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,从双缓冲区读出64字节的数据块,返回当前地址 */
{
	UINT8	i;
	Spi374Start( mAddr, CMD_SPI_374READ );
	for ( i = CH374_BLOCK_SIZE; i != 0; i -- ) *mBuf++ = Spi374InByte( );
	Spi374Stop( );
	return( mBuf );
}

PUINT8	CH374_WRITE_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,向双缓冲区写入64字节的数据块,返回当前地址 */
{
	UINT8	i;
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	for ( i = CH374_BLOCK_SIZE; i != 0; i -- ) Spi374OutByte( *mBuf++ );
	Spi374Stop( );
	return( mBuf );
}

void	CH374_WRITE_BLOCK_C( UINT8 mLen, PUINT8C mBuf )  /* 外部定义的被CH374程序库调用的子程序,向RAM_HOST_TRAN写入常量型数据块 */
{
	Spi374Start( RAM_HOST_TRAN, CMD_SPI_374WRITE );
	do {
		Spi374OutByte( *mBuf );
		mBuf ++;
	} while ( -- mLen );
	Spi374Stop( );
}
