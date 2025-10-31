# coding:utf-8
"""
Kylin Messenger - 配置文件管理模块
"""
import sys
import os
from enum import Enum

from PyQt5.QtCore import QLocale
from qfluentwidgets import (qconfig, QConfig, ConfigItem, OptionsConfigItem, BoolValidator,
                            OptionsValidator, RangeConfigItem, RangeValidator,
                            Theme, ConfigSerializer, __version__)


class Language(Enum):
    """ Language enumeration """
    CHINESE_SIMPLIFIED = QLocale(QLocale.Chinese, QLocale.China)
    CHINESE_TRADITIONAL = QLocale(QLocale.Chinese, QLocale.HongKong)
    ENGLISH = QLocale(QLocale.English)
    AUTO = QLocale()


class LanguageSerializer(ConfigSerializer):
    """ Language serializer """

    def serialize(self, language):
        return language.value.name() if language != Language.AUTO else "Auto"

    def deserialize(self, value: str):
        return Language(QLocale(value)) if value != "Auto" else Language.AUTO


def isWin11():
    return sys.platform == 'win32' and sys.getwindowsversion().build >= 22000


class Config(QConfig):
    """ Config of application """

    # main window
    micaEnabled = ConfigItem("MainWindow", "MicaEnabled", isWin11(), BoolValidator())
    dpiScale = OptionsConfigItem(
        "MainWindow", "DpiScale", "Auto", OptionsValidator([1, 1.25, 1.5, 1.75, 2, "Auto"]), restart=True)
    language = OptionsConfigItem(
        "MainWindow", "Language", Language.AUTO, OptionsValidator(Language), LanguageSerializer(), restart=True)

    # Material
    blurRadius = RangeConfigItem("Material", "AcrylicBlurRadius", 15, RangeValidator(0, 40))

    # user settings
    username = ConfigItem("User", "Username", "User", None)
    groupname = ConfigItem("User", "GroupName", "局域网", None)


# Application metadata
YEAR = 2024
AUTHOR = "Kylin Team"
VERSION = "1.0.0"
APP_NAME = "Kylin Messenger"
APP_DESCRIPTION = "A modern LAN messenger built with PyQt5 and Fluent Design"
GITHUB_URL = "https://github.com/your-repo/kylin"
FEEDBACK_URL = "https://github.com/your-repo/kylin/issues"
HELP_URL = "https://github.com/your-repo/kylin/wiki"

# Initialize config
cfg = Config()
cfg.themeMode.value = Theme.AUTO

# Load config file
config_dir = os.path.join(os.path.expanduser("~"), ".kylin")
config_file = os.path.join(config_dir, "config.json")
os.makedirs(config_dir, exist_ok=True)
qconfig.load(config_file, cfg)

