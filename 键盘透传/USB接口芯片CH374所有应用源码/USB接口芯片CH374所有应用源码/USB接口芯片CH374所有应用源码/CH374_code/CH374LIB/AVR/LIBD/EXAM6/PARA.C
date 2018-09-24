/* 并口接口子程序,供CH374子程序库调用 */

UINT8	CH374_READ_REGISTER( UINT8 mAddr )  /* 外部定义的被CH374程序库调用的子程序,从指定寄存器读取数据 */
{
	Write374Index( mAddr );
	return( Read374Data( ) );
}

void	CH374_WRITE_REGISTER( UINT8 mAddr, UINT8 mData )  /* 外部定义的被CH374程序库调用的子程序,向指定寄存器写入数据 */
{
	Write374Index( mAddr );
	Write374Data( mData );
}

void	CH374_READ_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,从指定起始地址读出数据块 */
{
	Write374Index( mAddr );
	while ( mLen -- ) *mBuf++ = Read374Data( );
}

void	CH374_WRITE_BLOCK( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,向指定起始地址写入数据块 */
{
	Write374Index( mAddr );
	while ( mLen -- ) Write374Data( *mBuf++ );
}

PUINT8	CH374_READ_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,从双缓冲区读出64字节的数据块,返回当前地址 */
{
	UINT8	i;
	Write374Index( mAddr );
	for ( i = CH374_BLOCK_SIZE / 4; i != 0; i -- ) {  /* 减少循环次数可以略微提高速度 */
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

PUINT8	CH374_WRITE_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,向双缓冲区写入64字节的数据块,返回当前地址 */
{
	UINT8	i;
	Write374Index( mAddr );
	for ( i = CH374_BLOCK_SIZE / 4; i != 0; i -- ) {  /* 减少循环次数可以略微提高速度 */
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

void	CH374_WRITE_BLOCK_C( UINT8 mLen, PUINT8C mBuf )  /* 外部定义的被CH374程序库调用的子程序,向RAM_HOST_TRAN写入常量型数据块 */
{
	Write374Index( RAM_HOST_TRAN );
	do {
		Write374Data( *mBuf );
		mBuf ++;
	} while ( -- mLen );
}
