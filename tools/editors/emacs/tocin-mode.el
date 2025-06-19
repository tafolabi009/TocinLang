;; tocin-mode.el --- Major mode for editing Tocin source code

;; Author: Tocin Team
;; Keywords: languages
;; Version: 0.1.0
;; Package-Requires: ((emacs "26.1") (lsp-mode "6.0") (company "0.9.13"))

;;; Commentary:
;; This package provides a major mode for editing Tocin source code.

;;; Code:

(require 'lsp-mode)
(require 'company)

(defgroup tocin nil
  "Major mode for editing Tocin source code."
  :group 'languages
  :prefix "tocin-")

(defcustom tocin-indent-offset 4
  "Number of spaces for each indentation step in `tocin-mode'."
  :type 'integer
  :safe 'integerp
  :group 'tocin)

;; Define syntax highlighting
(defvar tocin-font-lock-keywords
  `(("\\<\\(fn\\|let\\|if\\|else\\|while\\|for\\|return\\|break\\|continue\\)\\>" . font-lock-keyword-face)
    ("\\<\\(i32\\|i64\\|f32\\|f64\\|bool\\|string\\|char\\)\\>" . font-lock-type-face)
    ("\\<\\(true\\|false\\|null\\)\\>" . font-lock-constant-face)
    ("\\<[A-Z][A-Za-z0-9_]*\\>" . font-lock-type-face)
    ("\\<fn\\s-+\\([a-zA-Z_][a-zA-Z0-9_]*\\)" 1 font-lock-function-name-face)
    ("\\<let\\s-+\\([a-zA-Z_][a-zA-Z0-9_]*\\)" 1 font-lock-variable-name-face)))

;; Define indentation
(defun tocin-indent-line ()
  "Indent current line as Tocin code."
  (interactive)
  (let ((indent-col 0)
        (pos (- (point-max) (point)))
        beg)
    (save-excursion
      (beginning-of-line)
      (setq beg (point))
      (skip-chars-forward " \t")
      (if (looking-at "\\(?:}\\|)\\)")
          (save-excursion
            (forward-char)
            (backward-list)
            (setq indent-col (current-column)))
        (save-excursion
          (if (looking-at "\\(?:{\\|(\\)")
              (setq indent-col (current-column))
            (backward-up-list 1)
            (setq indent-col (+ (current-column) tocin-indent-offset)))))
      (if (> indent-col 0)
          (indent-line-to indent-col)
        (indent-line-to 0)))
    (if (> (- (point-max) pos) (point))
        (goto-char (- (point-max) pos)))))

;; Define syntax table
(defvar tocin-mode-syntax-table
  (let ((table (make-syntax-table)))
    ;; Comments
    (modify-syntax-entry ?/ ". 124" table)
    (modify-syntax-entry ?* ". 23b" table)
    (modify-syntax-entry ?\n ">" table)
    ;; Strings
    (modify-syntax-entry ?\" "\"" table)
    ;; Pairs
    (modify-syntax-entry ?\( "()" table)
    (modify-syntax-entry ?\) ")(" table)
    (modify-syntax-entry ?\{ "(}" table)
    (modify-syntax-entry ?\} "){" table)
    (modify-syntax-entry ?\[ "(]" table)
    (modify-syntax-entry ?\] ")[" table)
    table))

;; LSP configuration
(lsp-register-client
 (make-lsp-client
  :new-connection (lsp-stdio-connection
                  (lambda () `("node" ,(expand-file-name "~/.local/share/tocin/lsp/out/server.js") "--stdio")))
  :activation-fn (lsp-activate-on "tocin")
  :server-id 'tocin-ls
  :initialization-options (lambda () 
                          `(:formatting t
                            :diagnostics t
                            :semanticTokens t
                            :codeActions t))
  :multi-root t))

;;;###autoload
(define-derived-mode tocin-mode prog-mode "Tocin"
  "Major mode for editing Tocin source code."
  :syntax-table tocin-mode-syntax-table
  (setq-local font-lock-defaults '(tocin-font-lock-keywords))
  (setq-local indent-line-function 'tocin-indent-line)
  (setq-local comment-start "// ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "//+\\s-*")
  
  ;; Enable LSP
  (lsp)
  
  ;; Enable company-mode for completion
  (company-mode))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.to\\'" . tocin-mode))

(provide 'tocin-mode)

;;; tocin-mode.el ends here 