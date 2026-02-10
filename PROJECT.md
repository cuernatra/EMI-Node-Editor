# 1. Project Plan & Practices

This document defines the planned features and working practices of the project.

## 1.1 Table of Contents

- [1.2 Project Practices (Käytännöt)](#12-project-practices-käytännöt)
- [1.3 GitHub Issues & Workflow](#13-github-issues--workflow)
	- [1.3.1 Labels](#131-labels)
	- [1.3.2 Recommended Labels](#132-recommended-labels)
	- [1.3.3 Example Issue Structure](#133-example-issue-structure)
- [1.4 Code Style & Documentation](#14-code-style--documentation)
- [1.5 Planned Features & architecture](#15-planned-features--architecture)
	- [1.5.1 UI Layer](#151-ui-layer)
	- [1.5.2 Node Graph Layer](#152-node-graph-layer)
	- [1.5.3 Event System](#153-event-system)
	- [1.5.4 AST / Semantic Layer](#154-ast--semantic-layer)
	- [1.5.5 Persistence Layer](#155-persistence-layer)

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
| Code Style | https://google.github.io/styleguide/cppguide.html  |
| Doxygen | Public classes, structs, features and interfaces must have Doxygen comments. Template in `doxygenTemplate.txt`|
| TODOs | TODOs must reference a GitHub issue |

---

## 1.5 Planned Features & architecture

Linkt to Miro (architecture): https://miro.com/app/board/uXjVGG3LPcY=/

This section organizes planned features by project layer.

### 1.5.1 UI Layer

| Feature | Description | Done When |
|---------|------------|-----------|
| Node Rendering | Display nodes and ports | Nodes appear with correct size and ports |
| Node Interaction | Select, move, connect nodes | Drag/drop works, multiple selection works |

---

### 1.5.2 Node Graph Layer

| Feature | Description | Done When |
|---------|------------|-----------|
| Node Model | Typed inputs and outputs | Nodes have port definitions |
| Connections | Validated connections between nodes | Connection rules enforced |
| Graph Operations | Add/remove/move nodes | Operations update model correctly |
| Validation | Detect invalid nodes/connections | Invalid states flagged |
| Serialization | Save/load graph | Graph persists to disk correctly |


### 1.5.3 Event System

? This is still undecided, how will changes be updated

| Feature | Description | Done When |
|---------|------------|-----------|
| Change Events | Graph emits events on changes | All node/connection changes emit events |
| Subscription | Other systems can listen | UI & AST can subscribe to events |
| No Side Effects | Event system does not alter state | All updates go through the controller |


### 1.5.4 AST / Semantic Layer

| Feature | Description | Done When |
|---------|------------|-----------|
| Graph → AST | Convert graph into AST | Nodes are mapped correctly to AST nodes |
| Validation | AST detects semantic errors | Invalid graphs produce errors |
| AST Updates | Changes in graph update AST | AST syncs automatically with graph changes |


### 1.5.5 Persistence Layer

| Feature | Description | Done When |
|---------|------------|-----------|
| Save Graph | Write graph to file | Graph can be saved |
| Load Graph | Read graph from file | Graph can be restored correctly |
