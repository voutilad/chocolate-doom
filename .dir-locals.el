;; NOTE: This assumes you have flycheck and projectile!
((c-mode . ((c-file-style . "bsd")
             (indent-tabs-mode . nil)
             (tab-width . 8)
             (c-basic-offset . 4)
             (eval . (let* ((paths '(""
                                     "midiproc"
                                     "opl"
                                     "pcsound"
                                     "textscreen"
                                     "src"
                                     "src/doom"
                                     "src/heretic"
                                     "src/hexen"
                                     "src/strife"
                                     "src/setup"))
                            (full-paths (mapcar
                                         (lambda (path)
                                           (expand-file-name path (projectile-project-root)))
                                         paths)))
                       (setq-local flycheck-gcc-include-path
                             (append flycheck-gcc-include-path full-paths))
                       (setq-local flycheck-clang-include-path
                             (append flycheck-clang-include-path full-paths)))))))
