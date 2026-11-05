# Enforces "LoomCore has no UI code" (CLAUDE.md hard rule 2) at configure time.
# Rule (Specs/m1-core.md, "Toolchain"): LoomCore links Qt6::Core and toml++ only, and the
# configure fails if anything adds Qt Gui, Widgets, WebEngine or Pdf to it, directly or through
# another target. Tested by Tests/CMake/CoreLinkCheck.

# Stops the configure if a target links a forbidden Qt module.
#   target: the target to check (LoomCore).
# Walks the target's LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES, then those of every target
# they name, so a module pulled in second-hand is caught too. A forbidden module is any entry
# naming Qt6::Gui*, Qt6::Widgets*, Qt6::WebEngine* or Qt6::Pdf* (Pdf and PdfWidgets: M2's
# "Dependencies and toolchain"), or the versionless Qt:: names, including inside generator
# expressions such as $<LINK_ONLY:...>.
# Side effects: message(FATAL_ERROR) on the first forbidden module found.
function(loom_check_no_ui_links target)
    set(forbidden "(^|[^A-Za-z0-9_])(Qt6?::(Gui|Widgets|WebEngine|Pdf)[A-Za-z0-9_]*)")
    set(seen "")
    set(queue "${target}")
    while(queue)
        list(POP_FRONT queue current)
        if(current IN_LIST seen)
            continue()
        endif()
        list(APPEND seen "${current}")

        set(links "")
        foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(value "${current}" ${property})
            if(value)
                list(APPEND links ${value})
            endif()
        endforeach()

        foreach(item IN LISTS links)
            if(item MATCHES "${forbidden}")
                set(via "")
                if(NOT current STREQUAL target)
                    set(via " (through ${current})")
                endif()
                message(FATAL_ERROR
                    "${target} must not link ${CMAKE_MATCH_2}${via}. ${target} has no UI code: "
                    "it links Qt6::Core and toml++ only (CLAUDE.md hard rule 2; Specs/m1-core.md, Toolchain).")
            endif()
            # A static library's private links appear in its interface as $<LINK_ONLY:name>.
            string(REGEX REPLACE "^\\$<LINK_ONLY:(.+)>$" "\\1" name "${item}")
            if(TARGET "${name}")
                list(APPEND queue "${name}")
            endif()
        endforeach()
    endwhile()
endfunction()
