/* 并口接口子程序,供CH374子程序库调用 */
/* 对于速度较快的单片机,可以由基础子程序CH374_READ_BLOCK和CH374_WRITE_BLOCK生成其它子程序 */

/* 基础子程序 */

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

/* 生成子程序 */

UINT8	CH374_READ_REGISTER( UINT8 mAddr )  /* 外部定义的被CH374程序库调用的子程序,从指定寄存器读取数据 */
{
	UINT8	dat;
	CH374_READ_BLOCK( mAddr, 1, &dat );
	return( dat );
}

void	CH374_WRITE_REGISTER( UINT8 mAddr, UINT8 mData )  /* 外部定义的被CH374程序库调用的子程序,向指定寄存器写入数据 */
{
	CH374_WRITE_BLOCK( mAddr, 1, &mData );
}

PUINT8	CH374_READ_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,从双缓冲区读出64字节的数据块,返回当前地址 */
{
	CH374_READ_BLOCK( mAddr, 64, mBuf );
	return( mBuf + 64 );
}

PUINT8	CH374_WRITE_BLOCK64( UINT8 mAddr, PUINT8 mBuf )  /* 外部定义的被CH374程序库调用的子程序,向双缓冲区写入64字节的数据块,返回当前地址 */
{
	CH374_WRITE_BLOCK( mAddr, 64, mBuf );
	return( mBuf + 64 );
}

void	CH374_WRITE_BLOCK_C( UINT8 mLen, PUINT8C mBuf )  /* 外部定义的被CH374程序库调用的子程序,向RAM_HOST_TRAN写入常量型数据块 */
{
	CH374_WRITE_BLOCK( RAM_HOST_TRAN, mLen, mBuf );
}
