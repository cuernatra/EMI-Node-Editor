# 1. Project Plan & Practices

This document defines the planned features and working practices of the project.

## 1.1 Table of Contents

- [1.2 Project Practices (Käytännöt)](#12-project-practices-käytännöt)
- [1.3 GitHub Issues & Workflow](#13-github-issues--workflow)
  - [1.3.1 Labels](#131-labels)
  - [1.3.2 Recommended Labels](#132-recommended-labels)
  - [1.3.3 Example Issue Structure](#133-example-issue-structure)
- [1.4 Code Style & Documentation](#14-code-style--documentation)
- [1.5 Architecture](#15-architecture-and-features)

---

## 1.2 Project Practices (Käytännöt)

| Practice | Description |
|----|----|
| Design First | UI and architecture decisions are made before implementation |
| Issues as Truth | Features and decisions are tracked in GitHub issues |
| Minimal First | Implement simplest working solution first |
| Documentation | Document decisions, keep docs short and up to date |

---

## 1.3 GitHub Issues & Workflow

All features are tracked as GitHub issues.  
Each issue should follow these rules:

| Rule | Description |
|------|------------|
| Feature | Each feature is an issue. Can have sub-features as checklists or linked issues. |
| Sub-features | Optional sub-tasks can be included as checklists or separate issues. |
| Milestone | Each issue is assigned to a milestone (sprint). If no milestone, decide in the next meeting. |
| Responsibility | Each issue has a responsible person assigned. |
| Description | Should be clear and minimal: what the feature is and done conditions. |

### 1.3.1 Labels
Each issue has a label

### 1.3.2 Recommended Labels

| Label        | Purpose                                      |
|-------------|----------------------------------------------|
| design      | UI/UX decisions, mockups, layouts            |
| document    | Documentation or specs                        |
| feature     | New functionality                             |
| bug         | Bugs or regressions                            |
| important   | High-priority tasks                            |
| low-priority| Optional or nice-to-have tasks                |
| backend     | Core graph logic / AST / data                 |
| ui          | Frontend / node rendering / interaction      |
| refactor    | Code restructuring or cleanup                 |
| discussion  | Needs team discussion before implementation   |
| wontfix     | This will not be worked on           |




### 1.3.3 Example Issue Structure

**Title:** `Feature: Node Search`  

**Body:**
```md
### Feature
Node search in the editor.

### Done when
- [ ] Search bar implemented
- [ ] Results update dynamically as user types
- [ ] Works as overlay or window (decision made)
```


## 1.4 Code Style & Documentation

| Area | Practice |
|----|----|
| Code Style | https://micro-os-plus.github.io/develop/sutter-101/  |
| Doxygen | Public classes, structs, features and interfaces must have Doxygen comments. Template in `doxygenTemplate.txt`|
| TODOs | TODOs must reference a GitHub issue |

---

## 1.5 Architecture and Features

Linkt to Miro (architecture): https://miro.com/app/board/uXjVGG3LPcY=/

Features are listed in github issues.

