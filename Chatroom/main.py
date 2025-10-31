import sys
import os
from PyQt5.QtWidgets import QApplication
from PyQt5.QtCore import Qt
from ui.main_window import MainWindow
from qfluentwidgets import setTheme, Theme
from utils.config import cfg, APP_NAME

if __name__ == '__main__':
    """
    Kylin Messenger - 应用程序主入口
    """
    # 確保緩存目錄存在
    if not os.path.exists('cache'):
        os.makedirs('cache')
        
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    
    # 設置Fluent UI主題 (從配置讀取或使用系統主題)
    theme = cfg.get(cfg.themeMode)
    setTheme(theme)
    
    # 初始化並顯示主視窗
    main_win = MainWindow()
    main_win.show()
    
    sys.exit(app.exec_())