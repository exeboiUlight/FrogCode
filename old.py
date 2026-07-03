import sys
import os
import json
from pathlib import Path
import subprocess
import tkinter as tk
from tkinter import filedialog

# Настройки редактора
TAB_SIZE = 4

os.system('title FrogCode')

# Инициализация Tkinter для диалоговых окон
root = tk.Tk()
root.withdraw()

# Цвета терминала
def detect_terminal_colors():
    colors = {
        'reset': '\033[0m',
        'keyword': '\033[94m',
        'string': '\033[92m',
        'comment': '\033[90m',
        'number': '\033[93m',
        'operator': '\033[91m',
        'error': '\033[91m',
        'success': '\033[92m',
    }
    try:
        if os.name == 'posix':
            term_program = os.getenv('TERM_PROGRAM', '').lower()
            colorterm = os.getenv('COLORTERM', '').lower()
            if 'vscode' in term_program:
                colors.update({
                    'keyword': '\033[38;5;33m',
                    'string': '\033[38;5;34m',
                    'comment': '\033[38;5;245m',
                    'number': '\033[38;5;172m',
                    'operator': '\033[38;5;160m',
                })
            elif 'gnome-terminal' in term_program or 'truecolor' in colorterm:
                colors.update({
                    'keyword': '\033[38;2;114;159;207m',
                    'string': '\033[38;2;78;154;6m',
                    'comment': '\033[38;2;136;136;136m',
                    'number': '\033[38;2;196;160;0m',
                    'operator': '\033[38;2;204;0;0m',
                })
            elif 'iterm' in term_program:
                colors.update({
                    'keyword': '\033[38;5;75m',
                    'string': '\033[38;5;113m',
                    'comment': '\033[38;5;244m',
                    'number': '\033[38;5;221m',
                    'operator': '\033[38;5;203m',
                })
        elif os.name == 'nt':
            wt_session = os.getenv('WT_SESSION', '')
            if wt_session:
                colors.update({
                    'keyword': '\033[38;2;86;156;214m',
                    'string': '\033[38;2;78;201;176m',
                    'comment': '\033[38;2;106;153;85m',
                    'number': '\033[38;2;220;220;170m',
                    'operator': '\033[38;2;244;71;71m',
                })
    except Exception as e:
        print(f"Не удалось определить цвета терминала: {e}", file=sys.stderr)
    return colors

COLORS = detect_terminal_colors()

# Загрузка языков и правил подсветки
def load_languages():
    SYNTAX_RULES = {
        'python': {
            'keywords': ['def', 'class', 'if', 'else', 'elif', 'for', 'while', 'try', 'except', 
                        'finally', 'return', 'break', 'continue', 'import', 'from', 'as', 'with',
                        'lambda', 'pass', 'raise', 'yield', 'global', 'nonlocal', 'True', 'False',
                        'None', 'and', 'or', 'not', 'is', 'in', 'del'],
            'string_chars': ['"', "'"],
            'comment_chars': ['#'],
            'operators': ['+', '-', '*', '/', '=', '==', '!=', '<', '>', '<=', '>=', '%', '**', '//'],
            'start': 'python "{file}"'
        },
        'batch': {
            'keywords': ['@echo', 'off', 'on', 'set', 'if', 'else', 'for', 'in', 'do', 'goto', 
                         'call', 'exit', 'pause', 'rem', 'start', 'title', 'cd', 'dir', 'echo', 
                         'cls', 'del', 'copy', 'move', 'ren', 'md', 'rd', 'type', 'ver', 'vol'],
            'string_chars': ['"', "'"],
            'comment_chars': ['::', 'rem'],
            'operators': ['==', '=', '!='],
            'start': '"{file}"'
        }
    }
    lang_dir = Path('lang')
    lang_dir.mkdir(exist_ok=True)
    for lang_file in lang_dir.glob('*.json'):
        try:
            with open(lang_file, 'r', encoding='utf-8') as f:
                lang_data = json.load(f)
                lang_name = lang_file.stem.lower()
                SYNTAX_RULES[lang_name] = lang_data
                print(f"Загружен язык: {lang_name}")
        except Exception as e:
            print(f"Ошибка загрузки {lang_file}: {str(e)}")
    return SYNTAX_RULES

SYNTAX_RULES = load_languages()

def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')

def highlight_line(line, language):
    if language not in SYNTAX_RULES:
        return line
    line = line.replace('\t', ' ' * TAB_SIZE)
    rules = SYNTAX_RULES[language]
    highlighted = []
    i = 0
    n = len(line)
    while i < n:
        # Обработка комментариев
        for comment in rules['comment_chars']:
            if line.startswith(comment, i):
                highlighted.append(COLORS['comment'] + line[i:] + COLORS['reset'])
                return ''.join(highlighted)
        # Обработка строк
        for string_char in rules['string_chars']:
            if line.startswith(string_char, i):
                end = line.find(string_char, i + 1)
                if end == -1: end = n - 1
                highlighted.append(COLORS['string'] + line[i:end+1] + COLORS['reset'])
                i = end + 1
                break
        else:
            # Ключевые слова
            matched_keyword = False
            for keyword in rules['keywords']:
                if line.startswith(keyword, i) and (i + len(keyword) == n or not line[i + len(keyword)].isalnum()):
                    if i == 0 or not line[i-1].isalnum():
                        highlighted.append(COLORS['keyword'] + keyword + COLORS['reset'])
                        i += len(keyword)
                        matched_keyword = True
                        break
            if matched_keyword:
                continue
            # Операторы
            matched_operator = False
            for operator in rules['operators']:
                if line.startswith(operator, i):
                    highlighted.append(COLORS['operator'] + operator + COLORS['reset'])
                    i += len(operator)
                    matched_operator = True
                    break
            if matched_operator:
                continue
            # Остальное
            highlighted.append(line[i])
            i += 1
    return ''.join(highlighted)

def display_editor(filename, lines, cursor_pos, language, scroll_offset, visible_height):
    clear_screen()
    print(f"FrogCode запустил: {filename} | Язык: {language}")
    print("-" * 80)
    start = scroll_offset
    end = min(scroll_offset + visible_height, len(lines))
    for i in range(start, end):
        line_content = lines[i]
        cursor_col = cursor_pos[1]
        if i == cursor_pos[0]:
            if cursor_col > len(line_content):
                cursor_col = len(line_content)
            line_with_marker = (line_content[:cursor_col] + '|' + line_content[cursor_col:]) if line_content else '|'
            highlighted = highlight_line(line_with_marker, language)
        else:
            highlighted = highlight_line(line_content, language)
        prefix = "> " if i == cursor_pos[0] else "  "
        print(f"{prefix}{i+1:3}| {highlighted}")
    print("-" * 80)
    print(f"Строка: {cursor_pos[0]+1}, Позиция: {cursor_pos[1]} (Прокрутка: {scroll_offset})")

def get_key():
    if os.name == 'nt':
        import msvcrt
        ch = msvcrt.getch()
        if ch == b'\xe0':
            ch = msvcrt.getch()
            return {
                b'H': 'up',
                b'P': 'down',
                b'K': 'left',
                b'M': 'right',
                b'S': 'delete'
            }.get(ch, '')
        elif ch == b'\x03': return 'ctrl+c'
        elif ch == b'\x13': return 'ctrl+s'
        elif ch == b'\x11': return 'ctrl+q'
        elif ch == b'\x08': return 'backspace'
        elif ch == b'\r': return 'enter'
        elif ch == b'\t': return 'tab'
        elif ch == b'\x7f': return 'delete'
        elif ch == b'\x09': return 'ctrl+i'  # Ctrl+I для вставки файла
        else:
            try:
                return ch.decode('utf-8')
            except:
                return ''
    else:
        import tty
        import termios
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
            if ch == '\x1b':
                ch = sys.stdin.read(1)
                if ch == '[':
                    ch = sys.stdin.read(1)
                    return {
                        'A': 'up',
                        'B': 'down',
                        'C': 'right',
                        'D': 'left',
                        '3': 'delete'
                    }.get(ch, '')
            elif ch == '\x03': return 'ctrl+c'
            elif ch == '\x13': return 'ctrl+s'
            elif ch == '\x11': return 'ctrl+q'
            elif ch == '\x08' or ch == '\x7f': return 'backspace'
            elif ch == '\r': return 'enter'
            elif ch == '\t': return 'tab'
            elif ch == '\x09': return 'ctrl+i'  # Ctrl+I для вставки файла
            else: return ch
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

def detect_language(filename):
    ext = os.path.splitext(filename)[1].lower()
    lang_map = {
        '.py': 'python',
        '.bat': 'batch',
        '.cmd': 'batch',
        '.js': 'javascript',
        '.html': 'html',
        '.css': 'css',
        '.json': 'json',
        '.xml': 'xml',
        '.php': 'php',
        '.java': 'java',
        '.c': 'c',
        '.cpp': 'cpp',
        '.h': 'cpp',
        '.cs': 'csharp',
        '.go': 'go',
        '.rs': 'rust',
        '.sh': 'bash',
        '.qk': 'quark',
    }
    return lang_map.get(ext, 'text')

def run_code(filename, language):
    if language not in SYNTAX_RULES or 'start' not in SYNTAX_RULES[language]:
        print(f"{COLORS['error']}Не удалось запустить код: для языка '{language}' не указана команда запуска{COLORS['reset']}")
        input("Нажмите Enter...")
        return
    command = SYNTAX_RULES[language]['start'].format(file=filename)
    clear_screen()
    print(f"{COLORS['success']}Запускаем файл: {filename}{COLORS['reset']}")
    print(f"{COLORS['success']}Команда: {command}{COLORS['reset']}")
    print("-" * 80)
    try:
        process = subprocess.Popen(command, shell=True)
        process.wait()
    except Exception as e:
        print(f"{COLORS['error']}Ошибка при запуске: {str(e)}{COLORS['reset']}")
    input("\nНажмите Enter для возврата в редактор...")

def insert_file_content(lines, cursor_pos):
    """Вставляет содержимое файла в текущую позицию курсора"""
    try:
        # Открываем диалоговое окно выбора файла
        file_path = filedialog.askopenfilename(title="Выберите файл для вставки")
        if not file_path:
            return lines
        
        # Читаем содержимое файла
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Разделяем содержимое на строки
        inserted_lines = content.split('\n')
        
        # Вставляем содержимое в текущую позицию
        current_line = lines[cursor_pos[0]]
        before_cursor = current_line[:cursor_pos[1]]
        after_cursor = current_line[cursor_pos[1]:]
        
        # Обновляем текущую строку
        lines[cursor_pos[0]] = before_cursor + inserted_lines[0] if inserted_lines else before_cursor
        
        # Вставляем оставшиеся строки
        for i in range(1, len(inserted_lines)):
            lines.insert(cursor_pos[0] + i, inserted_lines[i])
        
        # Добавляем оставшуюся часть исходной строки
        if len(inserted_lines) > 1:
            lines[cursor_pos[0] + len(inserted_lines) - 1] += after_cursor
        else:
            lines[cursor_pos[0]] += after_cursor
        
        # Обновляем позицию курсора
        cursor_pos[0] += len(inserted_lines) - 1
        cursor_pos[1] = len(inserted_lines[-1]) if inserted_lines else 0
        
        return lines
    except Exception as e:
        print(f"{COLORS['error']}Ошибка при вставке файла: {str(e)}{COLORS['reset']}")
        input("Нажмите Enter...")
        return lines

def main():
    if len(sys.argv) > 1:
        filename = sys.argv[1]
        language = detect_language(filename)
    else:
        filename = input("Имя файла: ")
        language = detect_language(filename)
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            lines = [line.rstrip('\n') for line in f.readlines()]
    except FileNotFoundError:
        lines = ['']
    cursor_pos = [0, 0]
    visible_height = 20  # Кол-во строк, отображаемых
    scroll_offset = 0

    def update_scroll(cursor_pos, scroll_offset, visible_height, total_lines):
        # Обеспечить "прилипание" к курсору внутри окна
        if cursor_pos[0] < scroll_offset:
            scroll_offset = cursor_pos[0]
        elif cursor_pos[0] >= scroll_offset + visible_height:
            scroll_offset = cursor_pos[0] - visible_height + 1
        # Ограничение
        if scroll_offset < 0:
            scroll_offset = 0
        if scroll_offset > max(0, total_lines - visible_height):
            scroll_offset = max(0, total_lines - visible_height)
        return scroll_offset

    while True:
        scroll_offset = update_scroll(cursor_pos, scroll_offset, visible_height, len(lines))
        display_editor(filename, lines, cursor_pos, language, scroll_offset, visible_height)
        key = get_key()

        if key == 'up' and cursor_pos[0] > 0:
            cursor_pos[0] -= 1
            cursor_pos[1] = min(cursor_pos[1], len(lines[cursor_pos[0]]))
        elif key == 'down' and cursor_pos[0] < len(lines) - 1:
            cursor_pos[0] += 1
            cursor_pos[1] = min(cursor_pos[1], len(lines[cursor_pos[0]]))
        elif key == 'left' and cursor_pos[1] > 0:
            cursor_pos[1] -= 1
        elif key == 'right' and cursor_pos[1] < len(lines[cursor_pos[0]]):
            cursor_pos[1] += 1
        elif key == 'backspace':
            if cursor_pos[1] > 0:
                line = lines[cursor_pos[0]]
                lines[cursor_pos[0]] = line[:cursor_pos[1]-1] + line[cursor_pos[1]:]
                cursor_pos[1] -= 1
            elif cursor_pos[0] > 0:
                prev_line = lines[cursor_pos[0]-1]
                current_line = lines.pop(cursor_pos[0])
                cursor_pos[0] -= 1
                cursor_pos[1] = len(prev_line)
                lines[cursor_pos[0]] = prev_line + current_line
        elif key == 'delete':
            line = lines[cursor_pos[0]]
            if cursor_pos[1] < len(line):
                lines[cursor_pos[0]] = line[:cursor_pos[1]] + line[cursor_pos[1]+1:]
            elif cursor_pos[0] < len(lines) - 1:
                next_line = lines.pop(cursor_pos[0]+1)
                lines[cursor_pos[0]] = line + next_line
        elif key == 'enter':
            line = lines[cursor_pos[0]]
            new_line = line[cursor_pos[1]:]
            lines[cursor_pos[0]] = line[:cursor_pos[1]]
            lines.insert(cursor_pos[0]+1, new_line)
            cursor_pos[0] += 1
            cursor_pos[1] = 0
        elif key == 'ctrl+s':
            with open(filename, 'w', encoding='utf-8') as f:
                for line in lines:
                    f.write(line + '\n')
            print("\nФайл сохранен!")
            input("Нажмите Enter...")
        elif key == 'ctrl+g':
            run_code(filename, language)
        elif key == 'ctrl+q':
            clear_screen()
            break
        elif key == 'ctrl+i':  # Вставка файла
            lines = insert_file_content(lines, cursor_pos)
        elif key == 'tab':
            line = lines[cursor_pos[0]]
            lines[cursor_pos[0]] = line[:cursor_pos[1]] + '\t' + line[cursor_pos[1]:]
            cursor_pos[1] += 1
        elif isinstance(key, str) and len(key) == 1:
            line = lines[cursor_pos[0]]
            lines[cursor_pos[0]] = line[:cursor_pos[1]] + key + line[cursor_pos[1]:]
            cursor_pos[1] += 1

if __name__ == "__main__":
    main()