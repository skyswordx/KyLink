# coding:utf-8
"""
Kylin Messenger - 关于/作者界面
"""
from PyQt5.QtCore import Qt, QUrl, QRectF
from PyQt5.QtGui import QDesktopServices, QPainter, QColor, QBrush, QPainterPath, QLinearGradient
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QLabel, QHBoxLayout

from qfluentwidgets import (ScrollArea, SettingCardGroup, HyperlinkCard, 
                            PrimaryPushSettingCard, isDarkTheme, FluentIcon,
                            BodyLabel, TitleLabel, SubtitleLabel, IconWidget)

from utils.config import AUTHOR, VERSION, YEAR, APP_NAME, APP_DESCRIPTION, GITHUB_URL, FEEDBACK_URL, HELP_URL


class BannerWidget(QWidget):
    """ Banner widget for About page """

    def __init__(self, parent=None):
        super().__init__(parent=parent)
        self.setFixedHeight(200)

        self.vBoxLayout = QVBoxLayout(self)
        self.titleLabel = TitleLabel(APP_NAME, self)
        self.subtitleLabel = SubtitleLabel(APP_DESCRIPTION, self)

        self.vBoxLayout.setSpacing(10)
        self.vBoxLayout.setContentsMargins(36, 20, 36, 0)
        self.vBoxLayout.addWidget(self.titleLabel)
        self.vBoxLayout.addWidget(self.subtitleLabel)
        self.vBoxLayout.setAlignment(Qt.AlignLeft | Qt.AlignTop)

        self.titleLabel.setObjectName('titleLabel')

    def paintEvent(self, e):
        super().paintEvent(e)
        painter = QPainter(self)
        painter.setRenderHints(
            QPainter.SmoothPixmapTransform | QPainter.Antialiasing)
        painter.setPen(Qt.NoPen)

        path = QPainterPath()
        path.setFillRule(Qt.WindingFill)
        w, h = self.width(), self.height()
        path.addRoundedRect(QRectF(0, 0, w, h), 10, 10)
        path.addRect(QRectF(0, h-50, 50, 50))
        path.addRect(QRectF(w-50, 0, 50, 50))
        path.addRect(QRectF(w-50, h-50, 50, 50))
        path = path.simplified()

        # init linear gradient effect
        gradient = QLinearGradient(0, 0, 0, h)

        # draw background color
        if not isDarkTheme():
            gradient.setColorAt(0, QColor(207, 216, 228, 255))
            gradient.setColorAt(1, QColor(207, 216, 228, 0))
        else:
            gradient.setColorAt(0, QColor(0, 0, 0, 255))
            gradient.setColorAt(1, QColor(0, 0, 0, 0))

        painter.fillPath(path, QBrush(gradient))


class AuthorCard(QWidget):
    """ Author information card """

    def __init__(self, parent=None):
        super().__init__(parent=parent)
        self.setFixedHeight(120)

        self.hBoxLayout = QHBoxLayout(self)
        self.vBoxLayout = QVBoxLayout()

        # Avatar placeholder - 使用一个存在的图标
        try:
            avatar_icon = FluentIcon.ACCOUNT
        except AttributeError:
            try:
                avatar_icon = FluentIcon.CONTACT
            except AttributeError:
                avatar_icon = FluentIcon.INFO  # 使用 INFO 作为后备图标
        
        self.avatarWidget = IconWidget(avatar_icon, self)
        self.avatarWidget.setFixedSize(64, 64)

        # Author info
        self.nameLabel = TitleLabel(AUTHOR, self)
        self.roleLabel = SubtitleLabel("开发者", self)
        self.descriptionLabel = BodyLabel("基于 PyQt5 和 Fluent Design 构建的现代化局域网通讯工具", self)
        self.descriptionLabel.setWordWrap(True)

        self.hBoxLayout.setContentsMargins(24, 20, 24, 20)
        self.hBoxLayout.setSpacing(20)
        self.hBoxLayout.addWidget(self.avatarWidget)
        self.hBoxLayout.addLayout(self.vBoxLayout)
        
        self.vBoxLayout.setSpacing(4)
        self.vBoxLayout.addWidget(self.nameLabel)
        self.vBoxLayout.addWidget(self.roleLabel)
        self.vBoxLayout.addWidget(self.descriptionLabel)
        self.vBoxLayout.addStretch()


class AboutInterface(ScrollArea):
    """ About interface """

    def __init__(self, parent=None):
        super().__init__(parent=parent)
        self.scrollWidget = QWidget()
        self.expandLayout = QVBoxLayout(self.scrollWidget)

        # Banner
        self.banner = BannerWidget(self)

        # Author card
        self.authorCard = AuthorCard(self)

        # About group
        self.aboutGroup = SettingCardGroup(
            self.tr('关于'), self.scrollWidget)
        
        self.versionCard = PrimaryPushSettingCard(
            self.tr('版本') + f" {VERSION}",
            FluentIcon.INFO,
            self.tr('版本信息'),
            f'© {YEAR}, {AUTHOR}. ' + self.tr('版本') + f" {VERSION}",
            self.aboutGroup
        )

        self.helpCard = HyperlinkCard(
            HELP_URL,
            self.tr('打开帮助页面'),
            FluentIcon.HELP,
            self.tr('帮助'),
            self.tr('了解 Kylin Messenger 的功能和使用技巧'),
            self.aboutGroup
        )

        self.githubCard = HyperlinkCard(
            GITHUB_URL,
            self.tr('访问 GitHub'),
            FluentIcon.GITHUB,
            self.tr('GitHub 仓库'),
            self.tr('查看源代码和贡献代码'),
            self.aboutGroup
        )

        self.feedbackCard = PrimaryPushSettingCard(
            self.tr('提供反馈'),
            FluentIcon.FEEDBACK,
            self.tr('提供反馈'),
            self.tr('帮助我们改进 Kylin Messenger'),
            self.aboutGroup
        )

        self.__initWidget()

    def __initWidget(self):
        self.resize(1000, 800)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.setViewportMargins(0, 0, 0, 20)
        self.setWidget(self.scrollWidget)
        self.setWidgetResizable(True)
        self.setObjectName('aboutInterface')

        self.scrollWidget.setObjectName('scrollWidget')

        # Initialize layout
        self.__initLayout()
        self.__connectSignalToSlot()

    def __initLayout(self):
        self.expandLayout.setSpacing(28)
        self.expandLayout.setContentsMargins(0, 0, 0, 0)
        self.expandLayout.addWidget(self.banner)
        self.expandLayout.addWidget(self.authorCard)
        self.expandLayout.addWidget(self.aboutGroup)

        # Add cards to group
        self.aboutGroup.addSettingCard(self.versionCard)
        self.aboutGroup.addSettingCard(self.helpCard)
        self.aboutGroup.addSettingCard(self.githubCard)
        self.aboutGroup.addSettingCard(self.feedbackCard)

    def __connectSignalToSlot(self):
        """ connect signal to slot """
        self.feedbackCard.clicked.connect(
            lambda: QDesktopServices.openUrl(QUrl(FEEDBACK_URL)))

