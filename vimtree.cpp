#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <ncurses.h>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

struct Node {
    std::string path;
    std::string name;
    bool        is_dir;
    int         depth;
    bool        expanded = false;
    Node*       parent   = nullptr;
    std::vector<std::unique_ptr<Node>> children;

    Node(std::string p, std::string n, bool d, int depth_, Node* par)
        : path(std::move(p)), name(std::move(n)), is_dir(d), depth(depth_), parent(par) {}

    void load_children() {
        children.clear();
        if (!is_dir) return;
        try {
            std::vector<fs::directory_entry> entries;
            for (auto& e : fs::directory_iterator(path,
                    fs::directory_options::skip_permission_denied))
                entries.push_back(e);


            std::sort(entries.begin(), entries.end(),
                [](const fs::directory_entry& a, const fs::directory_entry& b) {
                    bool a_dir = a.is_directory();
                    bool b_dir = b.is_directory();
                    if (a_dir != b_dir) return a_dir > b_dir;
                    std::string an = a.path().filename().string();
                    std::string bn = b.path().filename().string();
                    std::transform(an.begin(), an.end(), an.begin(), ::tolower);
                    std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
                    return an < bn;
                });

            for (auto& e : entries) {
                std::string fname = e.path().filename().string();
                if (!fname.empty() && fname[0] == '.') continue;
                auto child = std::make_unique<Node>(
                    e.path().string(),
                    fname,
                    e.is_directory(),
                    depth + 1,
                    this
                );
                children.push_back(std::move(child));
            }
        } catch (...) {}
    }

    void toggle() {
        if (!is_dir) return;
        if (!expanded) {
            load_children();
            expanded = true;
        } else {
            expanded = false;
        }
    }

    void flat_list(std::vector<Node*>& out) {
        out.push_back(this);
        if (is_dir && expanded) {
            for (auto& c : children)
                c->flat_list(out);
        }
    }
};

std::unique_ptr<Node> build_root(const std::string& path) {
    fs::path abs = fs::absolute(path);
    std::string name = abs.filename().string();
    if (name.empty()) name = abs.string();
    auto root = std::make_unique<Node>(abs.string(), name, true, 0, nullptr);
    root->load_children();
    root->expanded = true;
    return root;
}

static std::string trunc(const std::string& s, int max_len) {
    if (max_len <= 0) return "";
    if ((int)s.size() <= max_len) return s;
    return s.substr(0, max_len);
}

static std::string ljust(const std::string& s, int w) {
    if ((int)s.size() >= w) return s;
    return s + std::string(w - s.size(), ' ');
}

static int find_node(const std::vector<Node*>& flat, const std::string& path) {
    for (int i = 0; i < (int)flat.size(); ++i)
        if (flat[i]->path == path) return i;
    return 0;
}

static void render_tree(const std::vector<Node*>& flat,
                         int cursor, int scroll,
                         const std::string& root_name)
{
    int h, w;
    getmaxyx(stdscr, h, w);
    erase();

    std::string header = " " + root_name + "  [up/dn jk] move  [Enter/Space] expand/open  [n] new file  [N] new folder  [q] quit ";
    header = trunc(header, w - 1);
    header = ljust(header, w - 1);
    attron(COLOR_PAIR(3) | A_BOLD);
    mvaddstr(0, 0, header.c_str());
    attroff(COLOR_PAIR(3) | A_BOLD);

    int list_height = h - 2;
    int end = std::min(scroll + list_height, (int)flat.size());

    for (int i = scroll; i < end; ++i) {
        int row      = i - scroll + 1;
        bool is_cur  = (i == cursor);
        Node* node   = flat[i];

        std::string indent(2 * (node->depth - 1), ' ');
        std::string label;
        if (node->is_dir) {
            std::string arrow = node->expanded ? "v " : "> ";
            label = indent + arrow + node->name;
        } else {
            label = indent + "  " + node->name;
        }
        label = trunc(label, w - 2);
        label = ljust(label, w - 1);

        int attr;
        if (is_cur)
            attr = COLOR_PAIR(1) | A_BOLD;
        else if (node->is_dir)
            attr = COLOR_PAIR(2);
        else
            attr = COLOR_PAIR(4);

        attron(attr);
        mvaddstr(row, 0, label.c_str());
        attroff(attr);
    }

    std::string footer = " " + std::to_string(cursor + 1) + "/" +
                         std::to_string(flat.size()) + " items ";
    footer = trunc(footer, w - 1);
    footer = ljust(footer, w - 1);
    attron(COLOR_PAIR(3));
    mvaddstr(h - 1, 0, footer.c_str());
    attroff(COLOR_PAIR(3));

    refresh();
}

static void open_in_vim(const std::string& path) {
    def_prog_mode();
    endwin();
    std::string cmd = "vim " + ("'" + path + "'");
    (void)std::system(cmd.c_str());
    reset_prog_mode();
    refresh();
    doupdate();
}

static std::string get_input(const std::string& prompt) {
    echo();
    curs_set(1);

    int h, w;
    getmaxyx(stdscr, h, w);
    std::string input;

    WINDOW* input_win = newwin(3, w - 2, h - 4, 1);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "%s: ", prompt.c_str());
    wrefresh(input_win);

    char buf[256] = {0};
    mvwgetnstr(input_win, 1, prompt.length() + 4, buf, 255);
    input = buf;

    delwin(input_win);
    noecho();
    curs_set(0);
    refresh();

    return input;
}

static bool create_item(Node* current_node, bool is_file, const std::string& name) {
    if (name.empty()) return false;

    fs::path target_dir;

    if (current_node->is_dir) {
        target_dir = current_node->path;
    } else {
        if (current_node->parent) {
            target_dir = current_node->parent->path;
        } else {
            target_dir = fs::path(current_node->path).parent_path();
        }
    }

    fs::path new_path = target_dir / name;

    try {
        if (is_file) {
            std::ofstream file(new_path);
            if (file) {
                file.close();
                return true;
            }
        } else {
            return fs::create_directory(new_path);
        }
    } catch (const std::exception& e) {
        return false;
    }
    return false;
}

static void refresh_tree(std::unique_ptr<Node>& root_owner, Node* root,
                         int& cursor, int& scroll) {
    if (root->expanded) {
        root->load_children();
    }

    std::vector<Node*> tmp_all;
    root->flat_list(tmp_all);
    std::vector<Node*> new_flat(tmp_all.begin() + 1, tmp_all.end());

    if (cursor >= (int)new_flat.size()) {
        cursor = new_flat.empty() ? 0 : new_flat.size() - 1;
    }

    int h, w;
    getmaxyx(stdscr, h, w);
    int list_height = h - 2;
    if (cursor < scroll)
        scroll = cursor;
    else if (cursor >= scroll + list_height)
        scroll = cursor - list_height + 1;
}

int main(int argc, char* argv[]) {
    std::string root_path = (argc > 1) ? argv[1] : ".";

    if (!fs::is_directory(root_path)) {
        fprintf(stderr, "Error: '%s' is not a directory.\n", root_path.c_str());
        return 1;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();

    init_pair(1, COLOR_BLACK,  COLOR_CYAN);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_BLACK,  COLOR_BLUE);
    init_pair(4, COLOR_WHITE,  -1);

    auto root_owner = build_root(root_path);
    Node* root = root_owner.get();

    int cursor = 0;
    int scroll  = 0;

    while (true) {
        std::vector<Node*> all;
        root->flat_list(all);
        std::vector<Node*> display_flat(all.begin() + 1, all.end());

        int h, w;
        getmaxyx(stdscr, h, w);
        int list_height = h - 2;

        if (!display_flat.empty()) {
            cursor = std::max(0, std::min(cursor, (int)display_flat.size() - 1));
        } else {
            cursor = 0;
        }

        if (cursor < scroll)
            scroll = cursor;
        else if (cursor >= scroll + list_height)
            scroll = cursor - list_height + 1;

        render_tree(display_flat, cursor, scroll, root->name);

        int key = getch();

        if (key == 'q' || key == 'Q') {
            break;

        } else if (key == KEY_UP || key == 'k') {
            cursor = std::max(0, cursor - 1);

        } else if (key == KEY_DOWN || key == 'j') {
            if (!display_flat.empty())
                cursor = std::min((int)display_flat.size() - 1, cursor + 1);

        } else if (key == KEY_PPAGE) {
            cursor = std::max(0, cursor - list_height);

        } else if (key == KEY_NPAGE) {
            if (!display_flat.empty())
                cursor = std::min((int)display_flat.size() - 1, cursor + list_height);

        } else if (key == '\n' || key == ' ' || key == KEY_ENTER || key == 'l') {
            if (display_flat.empty()) continue;
            Node* node = display_flat[cursor];
            if (node->is_dir) {
                node->toggle();
                std::vector<Node*> tmp_all;
                root->flat_list(tmp_all);
                std::vector<Node*> new_flat(tmp_all.begin() + 1, tmp_all.end());
                cursor = find_node(new_flat, node->path);
            } else {
                std::string sel = node->path;
                open_in_vim(sel);
                std::vector<Node*> tmp_all;
                root->flat_list(tmp_all);
                std::vector<Node*> new_flat(tmp_all.begin() + 1, tmp_all.end());
                cursor = find_node(new_flat, sel);
            }

        } else if (key == KEY_LEFT || key == 'h') {
            if (display_flat.empty()) continue;
            Node* node = display_flat[cursor];
            if (node->is_dir && node->expanded) {
                node->toggle();
                std::vector<Node*> tmp_all;
                root->flat_list(tmp_all);
                std::vector<Node*> new_flat(tmp_all.begin() + 1, tmp_all.end());
                cursor = find_node(new_flat, node->path);
            } else if (node->parent && node->parent != root) {
                std::vector<Node*> tmp_all;
                root->flat_list(tmp_all);
                std::vector<Node*> new_flat(tmp_all.begin() + 1, tmp_all.end());
                cursor = find_node(new_flat, node->parent->path);
            }

        } else if (key == KEY_RIGHT) {
            if (display_flat.empty()) continue;
            Node* node = display_flat[cursor];
            if (node->is_dir && !node->expanded) {
                node->toggle();
                std::vector<Node*> tmp_all;
                root->flat_list(tmp_all);
                std::vector<Node*> new_flat(tmp_all.begin() + 1, tmp_all.end());
                cursor = find_node(new_flat, node->path);
            }
        } else if (key == 'n') {
            if (display_flat.empty()) continue;
            Node* current_node = display_flat[cursor];
            
            std::string filename = get_input("Enter file name");
            if (!filename.empty()) {
                if (create_item(current_node, true, filename)) {
                    refresh_tree(root_owner, root, cursor, scroll);
                }
            }
        } else if (key == 'N') {
            if (display_flat.empty()) continue;
            Node* current_node = display_flat[cursor];
            
            std::string foldername = get_input("Enter folder name");
            if (!foldername.empty()) {
                if (create_item(current_node, false, foldername)) {
                    refresh_tree(root_owner, root, cursor, scroll);
                }
            }
        }
    }

    endwin();
    return 0;
}
