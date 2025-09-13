import os
import json

class EmojiManager:
    """
    一个简单的类，只负责加载表情地图并提供表情文件路径。
    所有与Qt显示相关的复杂逻辑都已被移除。
    """
    def __init__(self, emoji_map_path='resources/emojis/emoji_map.json', gifs_path='resources/emojis/gifs/'):
        self.emoji_map = {}
        self.gifs_path = gifs_path
        self.load_emoji_map(emoji_map_path)

    def load_emoji_map(self, path):
        try:
            # 构建绝对路径以确保在任何地方都能找到文件
            base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            full_path = os.path.join(base_dir, path)
            
            if not os.path.exists(full_path):
                print(f"错误: 表情地图路径不存在! {full_path}")
                return

            with open(full_path, 'r', encoding='utf-8') as f:
                self.emoji_map = json.load(f)
        except Exception as e:
            print(f"错误：无法加载表情地图 '{path}'. 原因: {e}")
            self.emoji_map = {}

    def get_emoji_path(self, code):
        """
        根据表情代码（如'[1]'）返回其GIF文件的完整路径。
        """
        filename = self.emoji_map.get(code)
        if filename:
            base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            return os.path.join(base_dir, self.gifs_path, filename)
        return None

    def get_all_emojis(self):
        """
        返回所有表情的代码和文件名映射。
        """
        return self.emoji_map

