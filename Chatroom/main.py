import sys
import os
from PyQt5.QtWidgets import QApplication
from ui.main_window import MainWindow

if __name__ == '__main__':
    """
    应用程序主入口
    """
    # 确保缓存目录存在
    if not os.path.exists('cache'):
        os.makedirs('cache')
        
    app = QApplication(sys.argv)
    
    # 初始化并显示主窗口
    main_win = MainWindow()
    main_win.show()
    
    sys.exit(app.exec_())