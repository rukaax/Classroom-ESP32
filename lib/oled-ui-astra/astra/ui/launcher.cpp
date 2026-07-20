/**
 * @file launcher.cpp
 * @brief astra UI启动器实现（已修改适配ESP32旋转编码器）
 * @brief 启动器是astra UI的核心控制器，负责：
 *   1. 管理菜单树的打开/关闭/切换
 *   2. 处理旋转编码器输入，实现菜单导航
 *   3. 驱动摄像机（Camera）和选择器（Selector）的动画
 *   4. 协调所有页面的渲染
 *
 * 与原版的主要修改：
 *   - 移除了硬编码的定时器测试逻辑
 *   - 将旋转编码器映射为菜单导航：
 *       左转(CLICK KEY_0) = 选择上一项
 *       右转(CLICK KEY_1) = 选择下一项
 *       长按(PRESS KEY_0) = 返回上级菜单
 *       长按(PRESS KEY_1) = 进入子菜单
 *   - 支持循环滚动（menuLoop配置项控制）
 */

#include "launcher.h"

namespace astra {

// ==================== 弹窗提示 ====================
/**
 * @brief 显示系统级弹窗（从屏幕上方滑入，停留后滑出）
 * @param _info 要显示的文字内容
 * @param _time 弹窗停留时间（帧数）
 *
 * 弹窗动画流程：
 *   1. 弹窗从屏幕上方滑入到屏幕中央偏上位置
 *   2. 停留指定帧数
 *   3. 滑出屏幕上方
 *
 * 注意：此函数会阻塞主循环直到弹窗动画结束
 */
void Launcher::popInfo(std::string _info, uint16_t _time) {
  static const uint64_t beginTime = this->time;
  static bool onRender = true;

  while (onRender) {
    time++;

    // 计算弹窗尺寸（文字宽度 + 边距，文字高度 + 边距）
    static float wPop = HAL::getFontWidth(_info) + 2 * getUIConfig().popMargin;
    static float hPop = HAL::getFontHeight() + 2 * getUIConfig().popMargin;

    // 弹窗Y坐标：从屏幕上方外开始
    static float yPop = 0 - hPop - 8;
    static float yPopTrg = 0;
    // 停留期间：目标位置在屏幕中央偏上（1/3处）
    if (time - beginTime < _time) yPopTrg = (HAL::getSystemConfig().screenHeight - hPop) / 3;
    // 超时后：滑出屏幕上方
    else yPopTrg = 0 - hPop - 8;

    // 弹窗X坐标：水平居中
    static float xPop = (HAL::getSystemConfig().screenWeight - wPop) / 2;

    // ---------- 渲染一帧 ----------
    HAL::canvasClear();
    currentPage->render(camera->getPosition());   // 渲染当前页面（作为背景）
    selector->render(camera->getPosition());       // 渲染选择器
    camera->update(currentPage, selector);         // 更新摄像机位置

    // 绘制弹窗：先画黑色背景框（遮挡背景），再画白色边框和文字
    HAL::setDrawType(0);  // 颜色0=黑色（清除像素，制造遮罩效果）
    HAL::drawRBox(xPop - 4, yPop - 4, wPop + 8, hPop + 8, getUIConfig().popRadius + 2);
    HAL::setDrawType(1);  // 颜色1=白色（点亮像素）
    HAL::drawRFrame(xPop - 1, yPop - 1, wPop + 2, hPop + 2, getUIConfig().popRadius);
    HAL::drawEnglish(xPop + getUIConfig().popMargin, yPop + getUIConfig().popMargin + HAL::getFontHeight(), _info);

    HAL::canvasUpdate();

    // 应用弹窗滑入/滑出动画
    animation(&yPop, yPopTrg, getUIConfig().popSpeed);

    // 按任意键提前关闭弹窗
    if (HAL::getAnyKey()) onRender = true;
    // 超时且弹窗已滑出屏幕 -> 结束弹窗
    if (time - beginTime >= _time && yPop == 0 - hPop - 8) onRender = false;
  }
}

// ==================== 菜单初始化 ====================
/**
 * @brief 初始化启动器（传入根菜单，建立菜单树）
 * @param _rootPage 根菜单指针
 *
 * 初始化内容：
 *   1. 设置当前页面为根菜单
 *   2. 创建摄像机（初始位置0,0）
 *   3. 初始化根菜单（计算各元素位置）
 *   4. 创建选择器并注入根菜单
 *   5. 选择器定位到第0项
 */
void Launcher::init(Menu *_rootPage) {
  currentPage = _rootPage;

  camera = new Camera(0, 0);
  _rootPage->init(camera->getPosition());

  selector = new Selector();
  selector->inject(_rootPage);
  selector->go(0);  // 选择器初始定位到第0项
}

// ==================== 打开子菜单 ====================
/**
 * @brief 打开当前选中的子菜单
 * @return true=成功打开，false=打开失败（无后继页面或页面为空）
 *
 * 打开流程：
 *   1. 检查当前选中项是否有子菜单
 *   2. 播放当前页面的退场动画
 *   3. 将当前页面指针移动到子菜单
 *   4. 初始化子菜单（进场动画）
 *   5. 将选择器注入子菜单
 */
bool Launcher::open() {
  if (currentPage->getNext() == nullptr) { popInfo("unreferenced page!", 600); return false; }
  if (currentPage->getNext()->getItemNum() == 0) { popInfo("empty page!", 600); return false; }

  currentPage->deInit();  // 播放退场动画

  currentPage = currentPage->getNext();  // 移动到子菜单
  currentPage->init(camera->getPosition());  // 初始化子菜单

  selector->inject(currentPage);  // 选择器注入新页面

  return true;
}

// ==================== 关闭子菜单（返回上级） ====================
/**
 * @brief 关闭当前页面，返回上级菜单
 * @return true=成功返回，false=返回失败（已是根菜单）
 *
 * 返回流程：
 *   1. 检查是否有前序页面（父菜单）
 *   2. 播放当前页面的退场动画
 *   3. 将当前页面指针移动到父菜单
 *   4. 重新初始化父菜单
 *   5. 将选择器注入父菜单
 */
bool Launcher::close() {
  if (currentPage->getPreview() == nullptr) { popInfo("unreferenced page!", 600); return false; }
  if (currentPage->getPreview()->getItemNum() == 0) { popInfo("empty page!", 600); return false; }

  currentPage->deInit();  // 播放退场动画

  currentPage = currentPage->getPreview();  // 返回父菜单
  currentPage->init(camera->getPosition());  // 重新初始化父菜单

  selector->inject(currentPage);  // 选择器注入父菜单

  return true;
}

// ==================== 主更新循环 ====================
/**
 * @brief 每帧调用一次，处理所有UI逻辑
 *
 * 执行流程：
 *   1. 清空画布
 *   2. 渲染当前页面（菜单项）
 *   3. 渲染选择器（高亮框）
 *   4. 更新摄像机位置（滚动动画）
 *   5. 扫描按键状态
 *   6. 处理旋转编码器输入：
 *       - 左转短按(CLICK KEY_0) = 选择上一个菜单项（支持循环）
 *       - 右转短按(CLICK KEY_1) = 选择下一个菜单项（支持循环）
 *       - 长按(PRESS KEY_0) = 返回上级菜单
 *       - 长按(PRESS KEY_1) = 进入选中的子菜单
 *   7. 刷新屏幕
 */
void Launcher::update() {
  // ---------- 渲染阶段 ----------
  HAL::canvasClear();                                    // 清空画布
  currentPage->render(camera->getPosition());           // 渲染当前页面的菜单项
  selector->render(camera->getPosition());               // 渲染选择器（高亮框+动画）
  camera->update(currentPage, selector);                 // 更新摄像机（处理页面滚动）

  // ---------- 输入处理阶段 ----------
  HAL::keyScan();  // 执行按键扫描状态机（消抖+长短按判断）

  if (HAL::getAnyKey()) {
    for (uint8_t i = 0; i < key::KEY_NUM; i++) {
      if (HAL::getKeyAction(static_cast<key::KEY_INDEX>(i)) == key::CLICK) {
        // ---------- 短按处理（选择菜单项） ----------
        if (i == 0) {
          // 左转：选择上一个菜单项
          if (currentPage->selectIndex > 0) {
            selector->go(currentPage->selectIndex - 1);  // 移动到上一项
          } else if (getUIConfig().menuLoop && currentPage->getItemNum() > 0) {
            selector->go(currentPage->getItemNum() - 1);  // 已在第一项，循环到最后一项
          }
        }
        if (i == 1) {
          // 右转：选择下一个菜单项
          if (currentPage->selectIndex < currentPage->getItemNum() - 1) {
            selector->go(currentPage->selectIndex + 1);  // 移动到下一项
          } else if (getUIConfig().menuLoop) {
            selector->go(0);  // 已在最后一项，循环到第一项
          }
        }
      } else if (HAL::getKeyAction(static_cast<key::KEY_INDEX>(i)) == key::PRESS) {
        // ---------- 长按处理（进入/返回子菜单） ----------
        if (i == 0) {
          close();  // 长按左键 = 返回上级菜单
        }
        if (i == 1) {
          open();   // 长按右键 = 进入选中的子菜单
        }
      }
    }
    HAL::clearKeyAction();  // 清除已处理的按键事件（防止重复触发）
  }

  // ---------- 刷新阶段 ----------
  HAL::canvasUpdate();  // 将画布内容发送到OLED屏幕

  time++;  // 帧计数器递增
}
}
