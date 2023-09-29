#include "ShaderGraph.h"
namespace tao_gizmos_shader_graph
{
    void TraverseShaderGraph(SGNode &root, std::function<bool(SGNode &)> func)
    {
        // process root
        if (!func(root)) return;

        // process root children
        for (int i = 0; i < root.ChildrenCount(); i++)
        {
            TraverseShaderGraph(root.ChildAt(i), func);
        }
    }

    void TraverseShaderGraph(SGNode &root, std::function<bool(SGNode &, int)> func, int stepsFromRoot)
    {
        // process root
        if (!func(root, stepsFromRoot)) return;

        stepsFromRoot++;

        // process root children
        for (int i = 0; i < root.ChildrenCount(); i++)
        {
            TraverseShaderGraph(root.ChildAt(i), func, stepsFromRoot);
        }
    }

    void TraverseShaderGraph(SGNode &root, std::function<bool(SGNode &, int, std::optional<SGNode *>)> func,
                             int stepsFromRoot, std::optional<SGNode *> prevNode)
    {
        // process root
        if (!func(root, stepsFromRoot, prevNode)) return;

        stepsFromRoot++;

        // process root children
        for (int i = 0; i < root.ChildrenCount(); i++)
        {
            TraverseShaderGraph(root.ChildAt(i), func, stepsFromRoot, &root);
        }
    }

    bool IsInputNode(SGNode &node)
    {
        return node.ChildrenCount() == 0;
    }

    std::string ParseShaderGraph(SGNode &outNode)
    {
        const char *kPre = "/* The following code was auto-generated */ \n";
        const char *kMainFuncStr = "void main()";

        /// Graph pre-process
        ////////////////////////////////////////////////////////////
        // The graph could have duplicates nodes,
        // the level of a node is the longest path to
        // reach it (could be more than one)
        std::map<SGNode *, int> nodeDepth;
        std::map<SGNode *, int> nodeRefCount;
        std::vector<SGNode *> inputNodes;

        // find the longest number of steps
        // to reach each node
        TraverseShaderGraph(outNode,
                            [&nodeDepth](SGNode &node, int steps) // Traversal func:
                            {
                                if (!nodeDepth.contains(&node))
                                    nodeDepth.insert(std::make_pair(&node, steps));

                                nodeDepth[&node] = std::max(nodeDepth[&node], steps);

                                return true;
                            });

        // Two things in this first traversal:
        // 1 - count the number of parents for a node
        // 2 - find leaf nodes
        TraverseShaderGraph(outNode,
                            [&nodeRefCount, &inputNodes](SGNode &node) // Traversal func:
                            {
                                if (!nodeRefCount.contains(&node))
                                {
                                    nodeRefCount.insert(std::make_pair(&node, 1));

                                    if (IsInputNode(node))
                                        inputNodes.push_back(&node);

                                    return true; // go on with the traversal
                                } else
                                {
                                    nodeRefCount[&node]++;
                                    return false; // break the traversal and continue from parent
                                }
                            });


        // We need to find all the NON-LEAF nodes that
        // have more than one parent, that means that
        // they need a name to be referenced multiple times.
        int cnt = 0;
        std::vector<std::pair<SGNode *, int>> namedNodes /* <node*, depth> */ ;
        std::for_each(nodeRefCount.begin(), nodeRefCount.end(),
                      [&namedNodes, &cnt, &nodeDepth](std::map<SGNode *, int>::value_type keyVal)
                      {
                          bool hasName = keyVal.first->Name.has_value();
                          bool needsName = keyVal.second > 1;

                          if ((hasName || needsName) &&
                              !IsInputNode(*keyVal.first))  // leaf nodes are inputs and are handled separately
                          {
                              int depth = nodeDepth[keyVal.first];
                              if (needsName)
                                  keyVal.first->Name = std::format("sgVar_{}", std::to_string(cnt++));

                              namedNodes.push_back({keyVal.first, depth});
                          }
                      });

        // sort the named nodes from closer to
        // the root to most distant
        std::sort(
                namedNodes.begin(),
                namedNodes.end(),
                [](const std::vector<std::pair<SGNode *, int>>::value_type &keyVal0,
                   const std::vector<std::pair<SGNode *, int>>::value_type &keyVal1)
                {
                    return keyVal0.second < keyVal1.second;
                }
        );

        /// Code gen
        ////////////////////////////////////////////////////////////

        /// Body
        std::vector<std::string> glslLines;

        glslLines.push_back(outNode.GlslString());

        for (auto &node: namedNodes)
        {
            // TODO:  oddio che schifo.....?
            std::string name = node.first->Name.value();
            node.first->Name = std::nullopt;
            glslLines.push_back(
                    std::format("{} {} = {};\n", node.first->GlslDeclString(), name, node.first->GlslString()));
            node.first->Name = name;

        }

        std::string body{};
        body.append(kMainFuncStr);
        body.append("\n{\n");
        for (int i = glslLines.size() - 1; i >= 0; i--) body.append(glslLines[i]);
        body.append("\n}\n");

        /// Input Block
        std::string inBlk{};
        for (auto &in: inputNodes)
        {
            if (in->Name.has_value())
            {
                inBlk.append(std::format("{} {} = ", in->GlslDeclString(), in->Name.value()));
                in->Name = std::nullopt;
                inBlk.append(std::format("{};\n", in->GlslString()));
            }
        }

        std::string res;
        res.append(kPre);
        res.append(inBlk);
        res.append(body);

        return res;
    }

    void ParseShaderGraphTGF(SGNode &outNode, const char *fileName)
    {
        std::map<SGNode *, int> uniqueNodesIds{};
        std::map<SGNode *, std::string> uniqueNodesLabels{};
        std::map<std::pair<int, int>, std::string> links{};

        int cnt = 0;
        TraverseShaderGraph(outNode, [&links, &uniqueNodesLabels, &uniqueNodesIds, &cnt](SGNode &node, int level,
                                                                                         std::optional<SGNode *> prevNode)
        {
            if (!uniqueNodesLabels.contains(&node))
            {
                uniqueNodesIds.insert({&node, cnt});
                uniqueNodesLabels.insert({&node, node.TgfLabel()});

                cnt++;
            }

            if (prevNode.has_value())
            {
                std::pair<int, int> link = {uniqueNodesIds[prevNode.value()], uniqueNodesIds[&node]};
                links[link] = std::format("{} {}", link.first, link.second);
            }

            return true;
        });

        std::string tgfString{};

        for (const auto &n: uniqueNodesIds)
        {
            tgfString.append(std::format("{} {}\n", n.second, uniqueNodesLabels[n.first]));
        }

        tgfString.append("#\n");

        for (const auto &l: links)
        {
            tgfString.append(l.second);
            tgfString.append("\n");
        }

        // write to file
        std::ofstream MyFile(fileName);
        MyFile << tgfString;
        MyFile.close();
    };

}