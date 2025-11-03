# coding:utf-8
"""
Kylin Messenger - 关于/作者界面
"""
from PyQt5.QtCore import Qt, QUrl, QRectF
from PyQt5.QtGui import QDesktopServices, QPainter, QColor, QBrush, QPainterPath, QLinearGradient
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QLabel, QHBoxLayout

from qfluentwidgets import (ScrollArea, SettingCardGroup, HyperlinkCard, 
                            PrimaryPushSettingCard, isDarkTheme, FluentIcon,
                            BodyLabel, TitleLabel, SubtitleLabel, IconWidget,
                            SimpleCardWidget)

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

        # Avatar placeholder - 使用开发者/代码图标，避免与导航栏重复
        try:
            avatar_icon = FluentIcon.CODE  # 使用代码图标表示开发者
        except AttributeError:
            try:
                avatar_icon = FluentIcon.DOCUMENT
            except AttributeError:
                avatar_icon = FluentIcon.HELP  # 使用 HELP 作为后备图标
        
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


class RoadmapCard(SimpleCardWidget):
    """ 功能计划卡片 """
    
    def __init__(self, parent=None):
        super().__init__(parent=parent)
        self.vBoxLayout = QVBoxLayout(self)
        self.vBoxLayout.setContentsMargins(20, 20, 20, 20)
        self.vBoxLayout.setSpacing(12)
        
        # 标题
        title_label = SubtitleLabel(self.tr('未来功能计划'), self)
        self.vBoxLayout.addWidget(title_label)
        
        # 功能列表
        features = [
            {
                'icon': FluentIcon.SPEED_HIGH if hasattr(FluentIcon, 'SPEED_HIGH') else FluentIcon.SETTING,
                'title': self.tr('NPU 加速支持'),
                'description': self.tr('启用 NPU（神经网络处理单元）进行图像处理、AI 功能加速，提升性能并降低 CPU 负载')
            },
            {
                'icon': FluentIcon.CAMERA if hasattr(FluentIcon, 'CAMERA') else FluentIcon.PHOTO,
                'title': self.tr('智能图像处理'),
                'description': self.tr('基于 NPU 的实时图像增强、背景虚化、美颜等功能')
            },
            {
                'icon': FluentIcon.CHAT if hasattr(FluentIcon, 'CHAT') else FluentIcon.MESSAGE,
                'title': self.tr('AI 消息助手'),
                'description': self.tr('本地 AI 模型支持，提供智能回复、消息摘要、翻译等功能')
            },
            {
                'icon': FluentIcon.SEARCH if hasattr(FluentIcon, 'SEARCH') else FluentIcon.FILTER,
                'title': self.tr('消息搜索优化'),
                'description': self.tr('使用 NPU 加速的全文搜索和语义搜索功能')
            },
            {
                'icon': FluentIcon.UPDATE if hasattr(FluentIcon, 'UPDATE') else FluentIcon.SYNC,
                'title': self.tr('性能优化'),
                'description': self.tr('持续优化网络传输、文件传输和 UI 渲染性能')
            }
        ]
        
        for feature in features:
            feature_layout = QHBoxLayout()
            feature_layout.setSpacing(12)
            
            # 图标
            try:
                icon = IconWidget(feature['icon'], self)
                icon.setFixedSize(32, 32)
            except:
                icon = QWidget()
                icon.setFixedSize(32, 32)
            
            # 文字内容
            text_layout = QVBoxLayout()
            text_layout.setSpacing(4)
            
            feature_title = BodyLabel(feature['title'], self)
            feature_title.setStyleSheet("font-weight: 600;")
            
            feature_desc = BodyLabel(feature['description'], self)
            feature_desc.setWordWrap(True)
            feature_desc.setStyleSheet("color: gray;" if not isDarkTheme() else "color: rgb(180, 180, 180);")
            
            text_layout.addWidget(feature_title)
            text_layout.addWidget(feature_desc)
            
            feature_layout.addWidget(icon)
            feature_layout.addLayout(text_layout, 1)
            
            self.vBoxLayout.addLayout(feature_layout)
        
        # 底部提示
        footer_label = BodyLabel(
            self.tr('以上功能正在开发中，预计将在后续版本中推出。'),
            self
        )
        footer_label.setWordWrap(True)
        footer_label.setStyleSheet("color: gray; font-style: italic;" if not isDarkTheme() else "color: rgb(150, 150, 150); font-style: italic;")
        self.vBoxLayout.addSpacing(8)
        self.vBoxLayout.addWidget(footer_label)


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

        # Roadmap card
        self.roadmapCard = RoadmapCard(self.scrollWidget)

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
        self.expandLayout.addWidget(self.roadmapCard)
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

