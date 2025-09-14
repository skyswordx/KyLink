import os
import json

class EmojiManager:
    """
    一個簡單的類，只負責加載表情地圖並提供表情文件路徑。
    所有與Qt顯示相關的複雜邏輯都已被移除。
    """
    def __init__(self, emoji_map_path='resources/emojis/emoji_map.json', gifs_path='resources/emojis/gifs/'):
        # 專案根目錄的絕對路徑
        self.base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        
        self.emoji_map = {}
        self.gifs_path = os.path.join(self.base_dir, gifs_path)
        
        self.load_emoji_map(os.path.join(self.base_dir, emoji_map_path))

    def load_emoji_map(self, full_path):
        try:
            if not os.path.exists(full_path):
                print(f"[EmojiManager 錯誤] 表情地圖檔案不存在: {full_path}")
                return

            with open(full_path, 'r', encoding='utf-8') as f:
                self.emoji_map = json.load(f)
            print(f"[EmojiManager] 成功載入 {len(self.emoji_map)} 個表情。")
        except Exception as e:
            print(f"[EmojiManager 錯誤] 無法載入表情地圖 '{full_path}'. 原因: {e}")
            self.emoji_map = {}

    def get_emoji_path(self, code):
        """
        根據表情代碼（如'[1]'）返回其GIF文件的完整路徑。
        """
        filename = self.emoji_map.get(code)
        if filename:
            # 直接使用已經是絕對路徑的 gifs_path
            return os.path.join(self.gifs_path, filename)
        return None

    def get_all_emojis(self):
        """
        返回所有表情的代碼和文件名映射。
        """
        return self.emoji_map