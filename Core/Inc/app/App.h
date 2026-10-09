#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 应用层 C 接口：实现见 Core/Src/App.cpp（C++）。
 * 仅向 main.c 暴露初始化、主循环轮询与显示刷新钩子，业务逻辑封装在 App 类内部。 */
void App_Init(void);
void App_UpdateDisplay(void);
void App_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
