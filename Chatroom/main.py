import sys
import os
from PyQt5.QtWidgets import QApplication
from ui.main_window import MainWindow
from qfluentwidgets import setTheme, Theme

if __name__ == '__main__':
    """
    應用程式主入口
    """
    # 確保緩存目錄存在
    if not os.path.exists('cache'):
        os.makedirs('cache')
        
    app = QApplication(sys.argv)
    
    # 設置Fluent UI主題 (可以改為 Theme.LIGHT)
    setTheme(Theme.DARK)
    
    # 初始化並顯示主視窗
    main_win = MainWindow()
    main_win.show()
    
    sys.exit(app.exec_())