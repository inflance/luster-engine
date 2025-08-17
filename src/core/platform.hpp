// Copyright (c) Luster
#pragma once

#include <cstdint>

namespace luster
{
	namespace Platform
	{
		// 初始化底层平台/SDL。失败时抛出异常
		void init();
		// 关闭平台/SDL
		void shutdown();
		// 设置鼠标光标可见性
		void setCursorVisible(bool visible);
		// 睡眠毫秒
		void sleepMs(uint32_t milliseconds);
	}
}


